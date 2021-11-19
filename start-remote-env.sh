docker build -t clion/remote-c-env:0.5 -f Dockerfile.remote-c-env .
docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name clion_remote_env clion/remote-c-env:0.5
