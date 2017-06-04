FROM ubuntu:latest

COPY ./nestopia nestopia

CMD /bin/ls -alh
