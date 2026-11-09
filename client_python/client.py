# cliente grpc em python
# pede um pdf pro usuario, manda pro servidor e salva o pdf comprimido que volta
# baseado no codigo de exemplo do professor

import os

import grpc
import file_processor_pb2
import file_processor_pb2_grpc

# no docker o servidor se chama "server", fora dele e localhost
SERVER_ADDRESS = os.getenv("SERVER_ADDRESS", "localhost:50051")


def compress_pdf(stub, input_file_path, output_file_path):
    if not os.path.isfile(input_file_path):
        print(f"arquivo nao encontrado: {input_file_path}")
        return

    # le o arquivo e vai mandando em pedacos
    # o nome do arquivo vai so no primeiro pedaco
    def file_iterator():
        with open(input_file_path, 'rb') as f:
            primeiro = True
            while True:
                chunk = f.read(1024)
                if not chunk and not primeiro:
                    break
                request = file_processor_pb2.FileRequest(
                    file_content=file_processor_pb2.FileChunk(content=chunk)
                )
                if primeiro:
                    request.file_name = os.path.basename(input_file_path)
                    request.compress_pdf_params.CopyFrom(file_processor_pb2.CompressPDFRequest())
                    primeiro = False
                yield request
                if not chunk:
                    break

    # chamada rpc
    response_stream = stub.CompressPDF(file_iterator())
    try:
        with open(output_file_path, 'wb') as output_file:
            for response in response_stream:
                output_file.write(response.file_content.content)
        print(f"pdf comprimido e salvo em: {output_file_path}")
    except grpc.RpcError as e:
        print(f"erro grpc ao comprimir pdf: {e.details()}")
        os.remove(output_file_path)  # apaga o arquivo que ficou pela metade
    except Exception as e:
        print(f"erro ao salvar arquivo comprimido: {e}")


def run_client():
    os.makedirs("saida", exist_ok=True)

    with grpc.insecure_channel(SERVER_ADDRESS) as channel:
        stub = file_processor_pb2_grpc.FileProcessorServiceStub(channel)

        while True:
            print("\n1 - comprimir pdf")
            print("0 - sair")
            opcao = input("opcao: ").strip()

            if opcao == "1":
                input_pdf = input("caminho do pdf: ").strip()
                output_pdf = os.path.join("saida", "compressed_" + os.path.basename(input_pdf))
                compress_pdf(stub, input_pdf, output_pdf)
            elif opcao == "0":
                break
            else:
                print("opcao invalida")


if __name__ == '__main__':
    run_client()
