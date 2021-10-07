FROM ubuntu:latest

COPY data-bus-experiments-non-mkl.zip /

RUN apt update && apt install -y unzip g++ libopenblas64-serial-dev liblapacke-dev liblapacke64-dev libtbb-dev
RUN unzip data-bus-experiments-non-mkl.zip

WORKDIR /data-bus-experiments-non-mkl/tbb-runner
RUN bash compile-def.sh && ./a.out

WORKDIR /data-bus-experiments-non-mkl/execution-statistics
RUN bash compile-def.sh && ./a.out

WORKDIR /data-bus-experiments-non-mkl/collect-dispersion
RUN bash compile-def.sh && ./a.out

WORKDIR /data-bus-experiments-non-mkl/bus-bandwidth-collector/
RUN bash compile-def.sh && ./ManualCollector.out

RUN apt remove -y unzip g++ libopenblas64-serial-dev liblapacke-dev liblapacke64-dev libtbb-dev; \
    apt autoremove -y; apt clean; rm -rf /var/cache/apt/lists
