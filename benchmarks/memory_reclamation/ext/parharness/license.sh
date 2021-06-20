for i in *.py # or whatever other pattern...
do
  if ! grep -q Copyright $i
  then
    cat license-header.txt $i >$i.new && mv $i.new $i
  fi
done
