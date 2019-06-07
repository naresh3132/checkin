APP_USER=`whoami`
APP_NAME=ltpffpublisher
RUN_DIR=.

PROCESSCOUNT=`ps -elf | grep $APP_USER |grep $APP_NAME| egrep -v "$0|grep" |  wc -l`
if [ $PROCESSCOUNT -ne 0 ] 
then
  echo -e "------------------------------------------------\n\n"
  echo "Following processes are running For User: $APP_USER ,  $APP_NAME"
  echo "------------------------------------------------"
  ps -elf | grep $APP_USER | grep $APP_NAME| egrep -v "$0|grep"
  echo -e "------------------------------------------------\n\n"
  echo "Exiting without starting...."
  exit 0
fi

cd $RUN_DIR
if [ $? -ne 0 ]
then
    echo "there was an error while changind dir to $RUN_DIR"  
    exit -1
fi

./ltpffpublisher > LtpFFPublisher.out&
