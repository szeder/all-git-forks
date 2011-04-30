set number
set smartindent
set showmatch
set tabstop=4

set encoding=utf-8

set ambiwidth=double

augroup InsertHook
autocmd!
autocmd InsertEnter * highlight StatusLine guifg=#ccdc90 guibg=#2E4340
autocmd InsertLeave * highlight StatusLine guifg=#2E4340 guibg=#ccdc90
augroup END

autocmd BufNewFile,BufRead *.html.erb set filetype=html
