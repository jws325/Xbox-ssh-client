export PATH="/home/james/nxdk/bin:$PATH"; 
export NXDK_DIR="/home/james/nxdk"; 
source /home/james/nxdk/bin/activate; 
chmod +x fetch-deps.sh; ./fetch-deps.sh; cd ../; make NXDK_DIR="$HOME/nxdk" -j4