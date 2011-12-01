#!/bin/sh
for var in $(perl -ne 'print "$1\n" if /^#\s*define\s*SPH_SIZE_(\w+)/' *.h); do
	U=$(echo $var | tr a-z A-Z)
	(
		echo "#ifndef SPH_${U}_GIT_H"
		echo "#define SPH_${U}_GIT_H"
		echo
		stripped=$(echo $var | tr -d 0-9)
		if [ "$stripped" = "sha" ]; then
			stripped=sha2
		fi
		echo "#include \"sph_$stripped.h\""
		echo
		echo "#define HASH_OCTETS (SPH_SIZE_${var}/8)"
		echo
		echo "#define git_HASH_CTX sph_${var}_context"
		echo "#define git_HASH_Init sph_${var}_init"
		echo "#define git_HASH_Update sph_${var}"
		echo "#define git_HASH_Final(dst,ctx) sph_${var}_close(ctx,dst)"
		echo
		echo "#endif"
	)> sph-${var}-git.h
done

