# Serviço de Processamento de Arquivos com gRPC

Entrega 1: serviço **CompressPDF**. Servidor em C++ comprime o PDF com Ghostscript e devolve ao cliente em Python.

## Estrutura

- `proto/file_processor.proto` - definição dos serviços e mensagens
- `server_cpp/` - servidor gRPC (C++)
- `client_python/` - cliente gRPC (Python)
- `Dockerfile.server`, `Dockerfile.client`, `docker-compose.yml` - containers
- `arquivos/` - PDFs de teste
- `logs/server.log` - log gerado pelo servidor

## Como executar

```
./start.sh
docker compose exec client python3 client_python/client.py
```

No menu, escolher `1` e informar o caminho do PDF, por exemplo `arquivos/teste.pdf`.
O PDF comprimido fica em `saida/` e o log em `logs/server.log`.

Para parar: `docker compose down`
