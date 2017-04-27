
echo "$0: installing packages"
echo "$0: root required"
sudo true

for package in "${packages[@]}" ; do
	sudo apt-get install --yes "${package}"
done

sudo -k

echo "$0: done!"


