# cliente grpc em python
# pede um arquivo pro usuario, manda pro servidor e salva o resultado que volta
# entrega 1: comprimir pdf
# entrega 2: pdf pra txt, converter formato de imagem e redimensionar imagem

import os

import grpc
import file_processor_pb2
import file_processor_pb2_grpc

# no docker o servidor se chama "server", fora dele e localhost
SERVER_ADDRESS = os.getenv("SERVER_ADDRESS", "localhost:50051")


def processar_arquivo(rpc, input_file_path, output_file_path, **parametros):
    # rpc = qual servico chamar (stub.CompressPDF, stub.ResizeImage...)
    # parametros = os parametros do servico, ex: resize_image_params=...
    if not os.path.isfile(input_file_path):
        print(f"arquivo nao encontrado: {input_file_path}")
        return

    # le o arquivo e vai mandando em pedacos
    # o nome e os parametros vao so no primeiro pedaco
    def file_iterator():
        with open(input_file_path, 'rb') as f:
            primeiro = True
            while True:
                chunk = f.read(1024)
                if not chunk and not primeiro:
                    break
                if primeiro:
                    request = file_processor_pb2.FileRequest(
                        file_name=os.path.basename(input_file_path),
                        file_content=file_processor_pb2.FileChunk(content=chunk),
                        **parametros
                    )
                    primeiro = False
                else:
                    request = file_processor_pb2.FileRequest(
                        file_content=file_processor_pb2.FileChunk(content=chunk)
                    )
                yield request
                if not chunk:
                    break

    # chamada rpc
    response_stream = rpc(file_iterator())
    try:
        # junta os pedacos e so salva no final, quando tem certeza que deu certo
        dados = b""
        for response in response_stream:
            dados += response.file_content.content
        with open(output_file_path, 'wb') as output_file:
            output_file.write(dados)
        print(f"arquivo processado e salvo em: {output_file_path}")
    except grpc.RpcError as e:
        print(f"erro grpc: {e.details()}")
    except Exception as e:
        print(f"erro ao salvar arquivo: {e}")


# ---------- uma funcao pra cada servico ----------

def compress_pdf(stub, input_pdf):
    output = os.path.join("saida", "compressed_" + os.path.basename(input_pdf))
    processar_arquivo(stub.CompressPDF, input_pdf, output,
                      compress_pdf_params=file_processor_pb2.CompressPDFRequest())


def convert_to_txt(stub, input_pdf):
    nome = os.path.splitext(os.path.basename(input_pdf))[0]
    output = os.path.join("saida", nome + ".txt")
    processar_arquivo(stub.ConvertToTXT, input_pdf, output,
                      convert_to_txt_params=file_processor_pb2.ConvertToTXTRequest())


def convert_image_format(stub, input_img, formato):
    nome = os.path.splitext(os.path.basename(input_img))[0]
    output = os.path.join("saida", nome + "." + formato)
    processar_arquivo(stub.ConvertImageFormat, input_img, output,
                      convert_image_format_params=file_processor_pb2.ConvertImageFormatRequest(
                          output_format=formato))


def resize_image(stub, input_img, largura, altura):
    output = os.path.join("saida", "resized_" + os.path.basename(input_img))
    processar_arquivo(stub.ResizeImage, input_img, output,
                      resize_image_params=file_processor_pb2.ResizeImageRequest(
                          width=largura, height=altura))


def ler_numero(texto):
    valor = input(texto).strip()
    return int(valor) if valor.lstrip('-').isdigit() else 0


def run_client():
    os.makedirs("saida", exist_ok=True)

    with grpc.insecure_channel(SERVER_ADDRESS) as channel:
        stub = file_processor_pb2_grpc.FileProcessorServiceStub(channel)

        while True:
            print("\n1 - comprimir pdf")
            print("2 - converter pdf para txt")
            print("3 - converter formato de imagem")
            print("4 - redimensionar imagem")
            print("0 - sair")
            opcao = input("opcao: ").strip()

            if opcao == "1":
                compress_pdf(stub, input("caminho do pdf: ").strip())
            elif opcao == "2":
                convert_to_txt(stub, input("caminho do pdf: ").strip())
            elif opcao == "3":
                imagem = input("caminho da imagem: ").strip()
                formato = input("formato de saida (png, jpg, gif, bmp, webp, tiff): ").strip().lower()
                convert_image_format(stub, imagem, formato)
            elif opcao == "4":
                imagem = input("caminho da imagem: ").strip()
                largura = ler_numero("largura: ")
                altura = ler_numero("altura: ")
                resize_image(stub, imagem, largura, altura)
            elif opcao == "0":
                break
            else:
                print("opcao invalida")


if __name__ == '__main__':
    run_client()
