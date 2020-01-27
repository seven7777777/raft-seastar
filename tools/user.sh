# !/bin/bash

set -e

dnf install -y openssh-server passwd cracklib-dicts
adduser dev
passwd dev << EOF
dev12345678
dev12345678
EOF
chmod u+w /etc/sudoers
echo "dev        ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers
chmod u-w /etc/sudoers

ssh-keygen -A   # ssh for root
su dev
# ssh for dev
ssh-keygen << EOF



EOF