set number
"set smartindent
set showmatch
set tabstop=4
set expandtab

set encoding=utf-8
set fileencodings=ucs-bom,iso-2022-jp-3,iso-2022-jp,eucjp-ms,euc-jisx0213,euc-jp,sjis,cp932,utf-8
set ambiwidth=double

augroup InsertHook
autocmd!
autocmd InsertEnter * highlight StatusLine guifg=#ccdc90 guibg=#2E4340
autocmd InsertLeave * highlight StatusLine guifg=#2E4340 guibg=#ccdc90
augroup END

autocmd BufNewFile,BufRead *.html.erb set filetype=html
autocmd BufNewFile,BufRead *.html.twig set filetype=html
autocmd BufNewFile,BufRead *.php.cache set filetype=php
