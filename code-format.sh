astyle \
	--style=otbs \
	--attach-namespaces \
	--attach-classes \
	--indent=spaces=4 \
	--convert-tabs \
	--align-pointer=name \
	--align-reference=name \
	--keep-one-line-statements \
	--pad-header \
	--pad-oper \
	--pad-comma \
	--unpad-paren \
	--suffix=none \
	"$@" \
    --recursive "src/core/*.c" "src/core/*.h" "src/app/*.c" "src/app/*.h"