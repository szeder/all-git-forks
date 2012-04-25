# .zshrc

export LANG=ja_JP.UTF-8

# User specific aliases and functions
alias grep='grep --color=always'
alias less='~/less.sh'
alias ll='ls -al --color'

export LESSCHARSET=utf-8

alias report='git log --pretty="%h %an %ad %s" --author=suenami --since="2011-11-29 7:00" --until="2011-11-30 7:00" --reverse'
alias cd="pushd"
alias bd="popd"
alias sha1="~/sha1"

# aliases for git
alias g="git"
alias gad="git add"
alias gb="git branch -v"
alias gba="git branch -a"
alias gbr="git branch -r"
alias gc="git commit -v"
alias gco="git checkout"
alias gd="git diff"
alias gdc="git diff --cached"
alias gf="git fetch"
alias gg="git grep -n"
alias ggi="git grep -ni"
alias gl="git log --stat"
alias gll="git log --oneline"
alias glp="git log -p"
alias gls="git ls-files"
alias gm="git merge"
alias gst="git status"
alias gs="git show"
alias gsp="git stash pop"
alias gss="git stash save"
alias gssh="git stash show"
alias gsl="git stash list"
alias gPull="git pull"
alias up='cd ./$(git rev-parse --show-cdup)' # cd to current repo's root dir
alias gcb="git branch --no-color 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/\1 /'" # Show current branch

# history
HISTFILE=$HOME/.zsh-history
HISTSIZE=100000
SAVEHIST=100000
setopt extended_history
setopt share_history
function history-all { history -E 1 }

#
# thx. http://ho-ki-boshi.blogspot.com/2007/12/zsh.html
#
local LEFTC=$'%{\e[1;32m%}' #ターミナル.app 用
local RIGHTC=$'%{\e[1;34m%}' #ターミナル.app 用
#local LEFTC=$'%{\e[38;5;30m%}'
#local RIGHTC=$'%{\e[38;5;88m%}'
local DEFAULTC=$'%{\e[m%}'
PROMPT=$LEFTC"%U%%%u "$DEFAULTC
export RPROMPT=$RIGHTC"[$USER@$HOST:%~]"$DEFAULTC
PROMPT2="%_%% "
SPROMPT="%r is correct? [n,y,a,e]: "

# プロンプトのカラー表示を有効
autoload -U colors
colors

# デフォルトの補完機能を有効
autoload -U compinit
compinit

# disable Ctrl+s, Ctrl+q
#setopt not_flow_controll

# ディレクトリ記憶
setopt auto_pushd

# コマンド間違い
setopt correct
# コマンド予測
#autoload predict-on
#predict-on
