// servidor grpc em c++
// recebe um pdf do cliente, comprime com o ghostscript e devolve o pdf comprimido
// baseado no codigo de exemplo do professor

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

class FileProcessorServiceImpl final : public FileProcessorService::Service
{
public:
    Status CompressPDF(ServerContext *context,
                       ServerReaderWriter<FileResponse, FileRequest> *stream) override
    {
        // recebe o arquivo em pedacos e junta tudo
        // o nome do arquivo vem no primeiro pedaco
        FileRequest request;
        std::string file_name;
        std::string file_data;
        while (stream->Read(&request))
        {
            if (file_name.empty())
                file_name = request.file_name();
            file_data += request.file_content().content();
        }

        if (file_data.empty())
        {
            LogError("CompressPDF", file_name, "arquivo vazio.");
            return Status(StatusCode::INVALID_ARGUMENT, "o arquivo enviado esta vazio.");
        }

        // salva o arquivo recebido temporariamente no servidor
        std::string input_file_path = "/tmp/input_" + file_name;
        std::string output_file_path = "/tmp/output_" + file_name;

        std::ofstream input_file_stream(input_file_path, std::ios::binary);
        if (!input_file_stream)
        {
            LogError("CompressPDF", file_name, "falha ao criar arquivo temporario de entrada.");
            return Status(StatusCode::INTERNAL, "erro no servidor ao criar arquivo temporario.");
        }
        input_file_stream.write(file_data.data(), file_data.size());
        input_file_stream.close();

        // executa o gs (comando do enunciado)
        // o 2>&1 junta a mensagem de erro do gs na saida, pra guardar no log
        std::string command = "gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/ebook "
                              "-dNOPAUSE -dQUIET -dBATCH -sOutputFile=" +
                              output_file_path + " " + input_file_path + " 2>&1";

        std::string gs_output;
        int gs_result = rodar_comando(command, gs_output);

        if (gs_result != 0)
        {
            LogError("CompressPDF", file_name, "falha na compressao pdf. codigo de retorno: " +
                                                   std::to_string(gs_result) + ". saida: " + gs_output.substr(0, gs_output.find('\n')));
            std::remove(input_file_path.c_str());
            std::remove(output_file_path.c_str());
            return Status(StatusCode::INTERNAL, "falha ao comprimir pdf.");
        }

        // devolve o pdf comprimido pro cliente em pedacos
        std::ifstream output_file_stream(output_file_path, std::ios::binary);
        if (!output_file_stream)
        {
            LogError("CompressPDF", file_name, "falha ao abrir arquivo comprimido para envio.");
            std::remove(input_file_path.c_str());
            return Status(StatusCode::INTERNAL, "erro no servidor ao abrir arquivo comprimido.");
        }

        char buffer[1024];
        while (output_file_stream.read(buffer, sizeof(buffer)) || output_file_stream.gcount() > 0)
        {
            FileResponse response;
            response.set_file_name("compressed_" + file_name);
            response.set_success(true);
            response.mutable_file_content()->set_content(buffer, output_file_stream.gcount());
            stream->Write(response); // envia esse pedaco pro cliente
        }
        output_file_stream.close();

        LogSuccess("CompressPDF", file_name, "compressao pdf bem-sucedida.");

        std::remove(input_file_path.c_str()); // limpa os arquivos temporarios
        std::remove(output_file_path.c_str());
        return Status::OK;
    }

private:
    // roda o comando, guarda o que ele escreveu e devolve o codigo de retorno
    int rodar_comando(const std::string &command, std::string &output)
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
