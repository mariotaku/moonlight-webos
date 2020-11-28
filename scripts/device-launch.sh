#!/usr/bin/expect -f

spawn ssh lgtv -t "apps/usr/palm/applications/com.limelight.webos/moonlight"
expect "Enter passphrase for key *"
send "FA8A17\r"
interact