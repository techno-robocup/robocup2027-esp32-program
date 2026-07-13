rsync -avzc --delete \
  --filter=':- .gitignore' \
  --exclude='.git/' \
  --exclude='send.sh' \
  --exclude='fetch.sh' \
  --exclude='README.md' \
  . techno@techno-torques.local:robocup2027-esp32-program

# Trigger service restart on remote
ssh techno@techno-torques.local "touch /home/techno/restart.trigger"

