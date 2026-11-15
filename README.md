# Serviço de Processamento de Arquivos com gRPC

Servidor gRPC em C++ que processa arquivos enviados por um cliente em Python:

- **CompressPDF** (entrega 1): comprime PDF com Ghostscript (`gs`)
- **ConvertToTXT** (entrega 2): converte PDF em texto com `pdftotext`
- **ConvertImageFormat** (entrega 2): converte o formato da imagem com `convert` (ImageMagick)
- **ResizeImage** (entrega 2): redimensiona a imagem com `convert -resize`

## Estrutura

- `proto/file_processor.proto` - definição dos serviços e mensagens
- `server_cpp/` - servidor gRPC (C++)
- `client_python/` - cliente gRPC (Python)
- `Dockerfile.server`, `Dockerfile.client`, `docker-compose.yml` - containers
- `arquivos/` - arquivos de teste (`teste.pdf`, `texto.pdf`, `teste.jpg` e arquivos inválidos para os testes de erro)
- `logs/server.log` - log gerado pelo servidor

## Como executar

```
./start.sh
docker compose exec client python3 client_python/client.py
```

No menu, escolher o serviço e informar o caminho do arquivo, por exemplo `arquivos/teste.pdf` ou `arquivos/teste.jpg`.
Os arquivos processados ficam em `saida/` e o log em `logs/server.log`.

Se mudar o código, recriar os containers com `docker compose up -d --build`.

Para parar: `docker compose down`
