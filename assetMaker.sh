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
		echo "inline static const char* ASSET_$NAME   =\"assets/$1/$2/$f\";"
	done
	cd .. 
	) | column -t
}
for module in $(ls); do 
  if [[ -d "$module" ]]; then
	  echo "-> Updating module: $module"
	  cd $module
	  D="../asset_$module.hpp"
	  echo "#pragma once" > $D
	  generate $module shaders >> $D
	  generate $module textures >> $D
	  cd ..
  fi
done
