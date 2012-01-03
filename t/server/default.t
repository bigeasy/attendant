#!/bin/bash

echo "1..2"

. t/lib/ok.sh

result=$(echo " " | t/bin/server)
ok "server exited normally"
[ -z "$result" ]; ok "server state clean"
