# $Id: Makefile 194 2008-12-18 01:47:21Z tk $

################################################################################
# The make rule compiles the code and generates the mios32_bsl_*.inc file
################################################################################
all: STM32G031K8 

MBHP_DIPCOREF4:
	make -f Makefile.bsl_MBHP_DIPCOREF4 cleanall
	make -f Makefile.bsl_MBHP_DIPCOREF4
	mv project_build/project.bin project_MBHP_DIPCOREF4.bin
	perl gen_inc_file.pl project_MBHP_DIPCOREF4.bin ../../mios32/STM32F4xx/mios32_bsl_MBHP_DIPCOREF4.inc mios32_bsl_image mios32_bsl
	perl gen_inc_file.pl project_MBHP_DIPCOREF4.bin ../updater/bsl_image_MBHP_DIPCOREF4.inc bsl_image 0

STM32G030K6:
	make -f Makefile.bsl_STM32G030K6 cleanall
	make -f Makefile.bsl_STM32G030K6
	mv project_build/project.bin project_STM32G030K6.bin
	perl gen_inc_file.pl project_STM32G030K6.bin ../../mios32/STM32G0xx/mios32_bsl_STM32G030K6.inc mios32_bsl_image mios32_bsl -reduced_bsl
	perl gen_inc_file.pl project_STM32G030K6.bin ../updater/mios32_bsl_STM32G030K6.inc bsl_image 0 -reduced_bsl
	
STM32G031K8:
	make -f Makefile.bsl_STM32G031K8 cleanall
	make -f Makefile.bsl_STM32G031K8
	mv project_build/project.bin project_STM32G031K8.bin
	perl gen_inc_file.pl project_STM32G031K8.bin ../../mios32/STM32G0xx/mios32_bsl_STM32G031K8.inc mios32_bsl_image mios32_bsl -reduced_bsl
	perl gen_inc_file.pl project_STM32G031K8.bin ../updater/mios32_bsl_STM32G031K8.inc bsl_image 0 -reduced_bsl
	
STM32G050K8:
	make -f Makefile.bsl_STM32G050K8 cleanall
	make -f Makefile.bsl_STM32G050K8
	mv project_build/project.bin project_STM32G050K8.bin
	perl gen_inc_file.pl project_STM32G050K8.bin ../../mios32/STM32G0xx/mios32_bsl_STM32G050K8.inc mios32_bsl_image mios32_bsl -reduced_bsl
	perl gen_inc_file.pl project_STM32G050K8.bin ../updater/mios32_bsl_STM32G050K8.inc bsl_image 0 -reduced_bsl
	
STM32G070CB:
	make -f Makefile.bsl_STM32G070CB cleanall
	make -f Makefile.bsl_STM32G070CB
	mv project_build/project.bin project_STM32G070CB.bin
	perl gen_inc_file.pl project_STM32G070CB.bin ../../mios32/STM32G0xx/mios32_bsl_STM32G070CB.inc mios32_bsl_image mios32_bsl -reduced_bsl
	perl gen_inc_file.pl project_STM32G070CB.bin ../updater/mios32_bsl_STM32G070CB.inc bsl_image 0 -reduced_bsl


		
clean:
	make -f Makefile.bsl_STM32G050K8 clean

cleanall:
	make -f Makefile.bsl_STM32G050K8 cleanall
