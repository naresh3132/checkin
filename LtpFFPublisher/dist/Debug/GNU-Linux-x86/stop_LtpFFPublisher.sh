APP_NAME=ltpffpublisher
pkill $APP_NAME

sleep 2

DATE=`date +"%d%b%Y_%H%M"`

BACKUP_DIR=$(ls Backup | wc -l)
if [ $BACKUP_DIR -eq 0 ]
then
  mkdir Backup
fi
mv LtpFFPublisher.out* Backup/
