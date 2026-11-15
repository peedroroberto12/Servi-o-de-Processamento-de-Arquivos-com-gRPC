// servidor grpc em c++
// recebe arquivos do cliente, processa com ferramentas de linha de comando e devolve o resultado
// entrega 1: CompressPDF (gs)
// entrega 2: ConvertToTXT (pdftotext), ConvertImageFormat e ResizeImage (convert do imagemagick)
// baseado no codigo de exemplo

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <ctime>
#include <sys/wait.h>

#include <grpcpp/grpcpp.h>
#include "file_processor.grpc.pb.h" // arquivo gerado pelo protobuf

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using grpc::StatusCode;
using file_processor::FileProcessorService;
using file_processor::FileRequest;
using file_processor::FileResponse;

typedef ServerReaderWriter<FileResponse, FileRequest> Stream;

class FileProcessorServiceImpl final : public FileProcessorService::Service
{
public:
    // ---------- entrega 1 ----------

    Status CompressPDF(ServerContext *context, Stream *stream) override
    {
        FileRequest primeiro;
        std::string file_data;
        ReceberArquivo(stream, primeiro, file_data);
        std::string file_name = primeiro.file_name();

        std::string input = "/tmp/input_" + file_name;
        std::string output = "/tmp/output_" + file_name;
        std::string command = "gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/ebook "
                              "-dNOPAUSE -dQUIET -dBATCH -sOutputFile=" + output + " " + input;

        return Processar(stream, "CompressPDF", file_name, file_data,
                         input, output, command, "compressed_" + file_name);
    }

    // ---------- entrega 2 ----------

    Status ConvertToTXT(ServerContext *context, Stream *stream) override
    {
        FileRequest primeiro;
        std::string file_data;
        ReceberArquivo(stream, primeiro, file_data);
        std::string file_name = primeiro.file_name();

        // o arquivo de saida tem o mesmo nome, so que .txt
        std::string output_name = SemExtensao(file_name) + ".txt";
        std::string input = "/tmp/input_" + file_name;
        std::string output = "/tmp/output_" + output_name;
        std::string command = "pdftotext " + input + " " + output;

        return Processar(stream, "ConvertToTXT", file_name, file_data,
                         input, output, command, output_name);
    }

    Status ConvertImageFormat(ServerContext *context, Stream *stream) override
    {
        FileRequest primeiro;
        std::string file_data;
        ReceberArquivo(stream, primeiro, file_data);
        std::string file_name = primeiro.file_name();
        std::string formato = primeiro.convert_image_format_params().output_format();

        // so aceita os formatos da lista (isso tambem evita colocar qualquer coisa no comando)
        if (!FormatoValido(formato))
        {
            LogError("ConvertImageFormat", file_name, "formato de saida invalido: '" + formato + "'");
            return Status(StatusCode::INVALID_ARGUMENT, "formato de saida invalido.");
        }

        // o convert descobre o formato pela extensao do arquivo de saida
        std::string output_name = SemExtensao(file_name) + "." + formato;
        std::string input = "/tmp/input_" + file_name;
        std::string output = "/tmp/output_" + output_name;
        std::string command = "convert " + input + " " + output;

        return Processar(stream, "ConvertImageFormat", file_name, file_data,
                         input, output, command, output_name);
    }

    Status ResizeImage(ServerContext *context, Stream *stream) override
    {
        FileRequest primeiro;
        std::string file_data;
        ReceberArquivo(stream, primeiro, file_data);
        std::string file_name = primeiro.file_name();
        int largura = primeiro.resize_image_params().width();
        int altura = primeiro.resize_image_params().height();

        if (largura <= 0 || altura <= 0)
        {
            LogError("ResizeImage", file_name, "dimensoes invalidas: " +
                                                   std::to_string(largura) + "x" + std::to_string(altura));
            return Status(StatusCode::INVALID_ARGUMENT, "largura e altura precisam ser maiores que zero.");
        }

        std::string tamanho = std::to_string(largura) + "x" + std::to_string(altura);
        std::string input = "/tmp/input_" + file_name;
        std::string output = "/tmp/output_resized_" + file_name;
        std::string command = "convert " + input + " -resize " + tamanho + " " + output;

        return Processar(stream, "ResizeImage", file_name, file_data,
                         input, output, command, "resized_" + file_name);
    }

private:
    // ---------- partes que todos os servicos usam ----------

    // recebe o arquivo em pedacos e junta tudo
    // o primeiro pedaco e guardado porque e nele que vem o nome e os parametros
    void ReceberArquivo(Stream *stream, FileRequest &primeiro, std::string &file_data)
    {
        FileRequest request;
        bool eh_o_primeiro = true;
        while (stream->Read(&request))
        {
            if (eh_o_primeiro)
            {
                primeiro = request;
                eh_o_primeiro = false;
            }
            file_data += request.file_content().content();
        }
    }

    // o passo a passo igual pra todos: salva, roda o comando, devolve e limpa
    Status Processar(Stream *stream, const std::string &service, const std::string &file_name,
                     const std::string &file_data, const std::string &input_path,
                     const std::string &output_path, const std::string &command,
                     const std::string &output_name)
    {
        if (file_data.empty())
        {
            LogError(service, file_name, "arquivo vazio.");
            return Status(StatusCode::INVALID_ARGUMENT, "o arquivo enviado esta vazio.");
        }

        // salva o arquivo recebido temporariamente no servidor
        std::ofstream input_file_stream(input_path, std::ios::binary);
        if (!input_file_stream)
        {
            LogError(service, file_name, "falha ao criar arquivo temporario de entrada.");
            return Status(StatusCode::INTERNAL, "erro no servidor ao criar arquivo temporario.");
        }
        input_file_stream.write(file_data.data(), file_data.size());
        input_file_stream.close();

        // executa o comando e captura a saida (o 2>&1 junta as mensagens de erro)
        std::string saida;
        int resultado = RodarComando(command + " 2>&1", saida);

        if (resultado != 0)
        {
            LogError(service, file_name, "falha ao processar. codigo de retorno: " +
                                             std::to_string(resultado) + ". saida: " + saida.substr(0, saida.find('\n')));
            std::remove(input_path.c_str());
            std::remove(output_path.c_str());
            return Status(StatusCode::INTERNAL, "falha ao processar o arquivo.");
        }

        // devolve o arquivo processado pro cliente em pedacos
        std::ifstream output_file_stream(output_path, std::ios::binary);
        if (!output_file_stream)
        {
            LogError(service, file_name, "falha ao abrir arquivo processado para envio.");
            std::remove(input_path.c_str());
            return Status(StatusCode::INTERNAL, "erro no servidor ao abrir arquivo processado.");
        }

        char buffer[1024];
        while (output_file_stream.read(buffer, sizeof(buffer)) || output_file_stream.gcount() > 0)
        {
            FileResponse response;
            response.set_file_name(output_name);
            response.set_success(true);
            response.mutable_file_content()->set_content(buffer, output_file_stream.gcount());
            stream->Write(response); // envia esse pedaco pro cliente
        }
        output_file_stream.close();

        LogSuccess(service, file_name, "processado com sucesso, saida: " + output_name);

        std::remove(input_path.c_str()); // limpa os arquivos temporarios
        std::remove(output_path.c_str());
        return Status::OK;
    }

    // roda o comando, guarda o que ele escreveu e devolve o codigo de retorno
    int RodarComando(const std::string &command, std::string &output)
    {
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe)
            return -1;

        char linha[256];
        while (fgets(linha, sizeof(linha), pipe) != nullptr)
            output += linha;

        int status = pclose(pipe);
        return WEXITSTATUS(status);
    }

    // "foto.jpg" -> "foto"
    std::string SemExtensao(const std::string &nome)
    {
        size_t ponto = nome.find_last_of('.');
        return (ponto == std::string::npos) ? nome : nome.substr(0, ponto);
    }

    bool FormatoValido(const std::string &formato)
    {
        const std::string formatos[] = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "tiff"};
        for (const std::string &f : formatos)
            if (formato == f)
                return true;
        return false;
    }

    // ---------- log ----------

    void LogError(const std::string &service_name, const std::string &file_name, const std::string &message)
    {
        Log("ERROR", service_name, file_name, message);
    }

    void LogSuccess(const std::string &service_name, const std::string &file_name, const std::string &message)
    {
        Log("SUCCESS", service_name, file_name, message);
    }

    // escreve no server.log e no console
    void Log(const std::string &level, const std::string &service_name,
             const std::string &file_name, const std::string &message)
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&now_c, &now_tm);
        char timestamp[26];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &now_tm);

        std::ofstream log_file("logs/server.log", std::ios::app);
        if (log_file.is_open())
        {
            log_file << "[" << timestamp << "] " << level << " - Service: " << service_name
                     << ", File: " << file_name << ", Message: " << message << std::endl;
            log_file.close();
        }
        else
        {
            std::cerr << "falha ao abrir arquivo de log!" << std::endl;
        }

        std::cout << "[" << timestamp << "] " << level << " - Service: " << service_name
                  << ", File: " << file_name << ", Message: " << message << std::endl;
    }
};

void RunServer()
{
    std::string server_address("0.0.0.0:50051");
    FileProcessorServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "servidor grpc ouvindo em " << server_address << std::endl;
    server->Wait();
}

int main()
{
    RunServer();
    return 0;
}
