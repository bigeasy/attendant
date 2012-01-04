#!/bin/bash

echo "1..2"

. t/lib/ok.sh

result=$(echo " " | t/bin/server)
ok "server exited normally"
[ $result == "RUNNING" ]; ok "server state clean"
