PART_NAME=firmware
REQUIRE_IMAGE_METADATA=1

platform_check_image() {
	local board=$(board_name)

	case "$board" in
	glinet,gl-sft1200)
		nand_do_platform_check "$board" "$1"
		return $?
		;;
	*)
		return 0
		;;
	esac
}

platform_do_upgrade() {
	local board=$(board_name)

	case "$board" in
	glinet,gl-sft1200)
		nand_do_upgrade "$1"
		;;
	*)
		default_do_upgrade "$1"
		;;
	esac
}
