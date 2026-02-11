#!/bin/bash
# Zsh and Oh My Zsh 配置脚本
# 参考 GitHub devcontainers features/common-utils 配置

set -e

# 接收参数
USERNAME="${1:-vscode}"

echo ">>> 开始配置 Zsh 和 Oh My Zsh..."

# 设置 USER 变量和 PATH
echo 'if [ -z "${USER}" ]; then export USER=$(whoami); fi' >> /etc/profile.d/devcontainer-zsh-env.sh
echo 'if [[ "${PATH}" != *"$HOME/.local/bin"* ]]; then export PATH="${PATH}:$HOME/.local/bin"; fi' >> /etc/profile.d/devcontainer-zsh-env.sh
chmod +x /etc/profile.d/devcontainer-zsh-env.sh

# 安装 Oh My Zsh
export ZSH="/root/.oh-my-zsh"
umask g-w,o-w
mkdir -p ${ZSH}
git clone --depth=1 -c core.eol=lf -c core.autocrlf=false \
    -c fsck.zeroPaddedFilemode=ignore -c fetch.fsck.zeroPaddedFilemode=ignore \
    -c receive.fsck.zeroPaddedFilemode=ignore \
    "https://github.com/ohmyzsh/ohmyzsh" "${ZSH}" 2>&1

# 克隆后压缩 git 历史以减小体积
cd "${ZSH}"
git repack -a -d -f --depth=1 --window=1

# 安装常用插件
git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting

# 设置项目原有的配置选项
echo 'setopt NO_AUTO_REMOVE_SLASH' >> /root/.zshrc
mkdir -p ~/.oh-my-zsh/completions
echo 'source $HOME/.profile' >> /root/.zshrc
chsh -s $(which zsh)

# 创建自定义主题目录
mkdir -p /root/.oh-my-zsh/custom/themes

# 添加 GitHub devcontainers 的自定义主题
cat > /root/.oh-my-zsh/custom/themes/devcontainers.zsh-theme << 'THEME_EOF'
# Oh My Zsh! theme - partly inspired by https://github.com/ohmyzsh/ohmyzsh/blob/master/themes/robbyrussell.zsh-theme
__zsh_prompt() {
    local prompt_username
    if [ ! -z "${GITHUB_USER}" ]; then
        prompt_username="@${GITHUB_USER}"
    else
        prompt_username="%n"
    fi
    PROMPT="%{$fg[green]%}${prompt_username} %(?:%{$reset_color%}➜ :%{$fg_bold[red]%}➜ )"
    PROMPT+='%{$fg_bold[blue]%}%(5~|%-1~/…/%3~|%4~)%{$reset_color%} '
    PROMPT+='`\
    if [ "$(git config --get devcontainers-theme.hide-status 2>/dev/null)" != 1 ] && \
       [ "$(git config --get codespaces-theme.hide-status 2>/dev/null)" != 1 ]; then \
        export BRANCH=$(git --no-optional-locks symbolic-ref --short HEAD 2>/dev/null || git --no-optional-locks rev-parse --short HEAD 2>/dev/null); \
        if [ "${BRANCH}" != "" ]; then \
            echo -n "%{$fg_bold[cyan]%}(%{$fg_bold[red]%}${BRANCH}" \
            && if [ "$(git config --get devcontainers-theme.show-dirty 2>/dev/null)" = 1 ] && \
               git --no-optional-locks ls-files --error-unmatch -m --directory --no-empty-directory -o --exclude-standard ":/*" > /dev/null 2>&1; then \
                echo -n " %{$fg_bold[yellow]%}✗"; \
            fi \
            echo -n "%{$fg_bold[cyan]%})%{$reset_color%} "; \
        fi; \
    fi`'
    PROMPT+='%{$fg[white]%}$ %{$reset_color%}'
    unset -f __zsh_prompt
}
__zsh_prompt
THEME_EOF

# 创建符号链接 codespaces -> devcontainers
ln -sf /root/.oh-my-zsh/custom/themes/devcontainers.zsh-theme /root/.oh-my-zsh/custom/themes/codespaces.zsh-theme

# 配置 .zshrc 使用 devcontainers 主题
sed -i 's/ZSH_THEME=.*/ZSH_THEME="devcontainers"/g' /root/.zshrc

# 配置 .zshrc，在开头 source /etc/profile 和设置用户环境
sed -i '1isetopt NULL_GLOB 2>/dev/null\n\
source /etc/profile\n\
unsetopt NULL_GLOB 2>/dev/null\n\
if command -v docker > /dev/null; then\n\
    docker completion zsh > ~/.oh-my-zsh/completions/_docker\n\
fi' /root/.zshrc

# 设置终端标题（来自 GitHub devcontainers theme）
cat >> /root/.zshrc << 'TITLE_EOF'

# Check if the terminal is xterm
if [[ "$TERM" == "xterm" ]]; then
    # Function to set the terminal title to the current command
    preexec() {
        local cmd=${1}
        echo -ne "\\033]0;${USER}@${HOSTNAME}: ${cmd}\\007"
    }
    # Function to reset the terminal title to the shell type after the command is executed
    precmd() {
        echo -ne "\\033]0;${USER}@${HOSTNAME}: ${SHELL}\\007"
    }
    # Add the preexec and precmd functions to the corresponding hooks
    autoload -Uz add-zsh-hook
    add-zsh-hook preexec preexec
    add-zsh-hook precmd precmd
fi
TITLE_EOF

# 将 root 用户的 Oh My Zsh 配置复制到 vscode 用户（参考 GitHub devcontainers）
if [ "${USERNAME}" != "root" ]; then
    user_home="/home/${USERNAME}"
    mkdir -p ${user_home}
    cp -rf /root/.oh-my-zsh ${user_home}/
    cp -f /root/.zshrc ${user_home}/.zshrc
    chown -R ${USERNAME}:${USERNAME} ${user_home}/.oh-my-zsh ${user_home}/.zshrc
fi

echo ">>> Zsh 和 Oh My Zsh 配置完成！"
