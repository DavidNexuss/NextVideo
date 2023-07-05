#!/bin/bash

cd assets

function generate { 
	(
	echo "// $2"
	cd $2
	for f in $(ls); do
    		B=$(basename "$f")
		NAME=${B^^}
		NAME=${NAME/./_}
		echo "#define ASSET_$NAME   \"assets/$1/$2/$f\""
	done
	cd .. 
	) | column -t
}
for module in $(ls); do 
  if [[ -d "$module" ]]; then
	  echo "-> Updating module: $module"
	  cd $module
	  generate $module shaders > ../$module.hpp
	  generate $module textures >> ../$module.hpp
	  cd ..
  fi
done
