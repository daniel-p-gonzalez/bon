docker build -t bon . && docker run -it --mount src=%CD%\workspace,target=/bon,type=bind -w /bon bon /bin/bash -c "mkdir -p /bon/examples && cp /bon_build/examples/* /bon/examples/; /bin/bash"
