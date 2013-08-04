### How to build for iOS Platform?
./build.sh

### Installation
1. Open a terminal and copy git to your iOS device. Please make sure you have installed OpenSSH on your iOS device.

   scp ./git root@10.0.1.2:/usr/bin

   2. ssh your iOS device

      ssh root@10.0.1.2

      3. Change file mode of git after you have logged in iOS device
         
            chmod 0755 /usr/bin/git

### Important
Git for iOS supports SSH and GIT protocols, but it doesn’t support HTTPS protocol. You should use GIT protocol instead. 
(Replace https:// with git://)
