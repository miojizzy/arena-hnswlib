FROM debian:bookworm 

RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash && \
    apt-get update && \
    apt-get install -y rsync jq git git-lfs vim curl gettext-base g++-12 make cmake && \
    apt-get clean && \
    git lfs install && \
    rm -rf /var/lib/apt/lists/* && \
    rm /root/.gitconfig
  
RUN ln -sf /bin/bash /bin/sh




# FROM cnbcool/default-build-env:latest

# ENV UV_INSTALL_DIR=/usr/local/bin

# # install vscode and extension
# RUN curl -fsSL https://code-server.dev/install.sh | sh &&\
#     code-server --install-extension cnbcool.cnb-welcome &&\
#     code-server --install-extension redhat.vscode-yaml &&\
#     code-server --install-extension waderyan.gitblame &&\
#     code-server --install-extension mhutchie.git-graph &&\
#     code-server --install-extension donjayamanne.githistory &&\
#     code-server --install-extension cloudstudio.live-server &&\
#     code-server --install-extension tencent-cloud.coding-copilot &&\
#     code-server --install-extension bierner.markdown-mermaid
    

# # 安装 ssh 服务，用于支持 VSCode 客户端通过 Remote-SSH 访问开发环境，开发环境需保留 apt-get 缓存
# RUN apt-get update && apt-get install -y wget unzip lsof nload htop net-tools dnsutils openssh-server zsh

# # 安装 uv
# RUN sh -c "$(curl -LsSf https://astral.sh/uv/install.sh)" && uv python install

# COPY scripts ./scripts

# RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" && \
#     git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions && \
#     git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting && \
#     chmod +x ./scripts/*.sh && \
#     ./scripts/add-zsh-plugins.sh zsh-autosuggestions zsh-syntax-highlighting && \
#     ./scripts/set-zsh-env.sh && \
#     echo 'setopt NO_AUTO_REMOVE_SLASH' >> /root/.zshrc && \
#     mkdir -p ~/.oh-my-zsh/completions && \
#     chsh -s $(which zsh)

# # 在最开始 source /etc/profile 
# RUN sed -i '1isetopt NULL_GLOB 2>/dev/null\n\
# source /etc/profile\n\
# unsetopt NULL_GLOB 2>/dev/null\n\
# if command -v docker > /dev/null; then\n\
#     docker completion zsh > ~/.oh-my-zsh/completions/_docker\n\
# fi' $HOME/.zshrc


# COPY settings.json /root/.vscode-server/data/Machine/settings.json
# COPY settings.json /root/.local/share/code-server/Machine/settings.json
# COPY cnb-init-from /bin/cnb-init-from
# COPY cnb-init-from-without-lfs /bin/cnb-init-from-without-lfs

# # 指定字符集支持命令行输入中文（根据需要选择字符集）
# ENV LANG=C.UTF-8
# ENV LANGUAGE=C.UTF-8
