FROM python:3.12-slim

WORKDIR /imhex

COPY ./out/ .

EXPOSE 9090

CMD [ "python", "/imhex/serve.py" ]