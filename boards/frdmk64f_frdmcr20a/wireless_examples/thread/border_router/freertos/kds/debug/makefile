################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include utilities/subdir.mk
-include usb/osa/subdir.mk
-include usb/device/source/khci/subdir.mk
-include usb/device/source/subdir.mk
-include startup/subdir.mk
-include source/subdir.mk
-include nwk_ip/base/utils/subdir.mk
-include nwk_ip/base/thread_config/subdir.mk
-include nwk_ip/base/thci/subdir.mk
-include nwk_ip/base/stack_globals/subdir.mk
-include nwk_ip/base/shell/subdir.mk
-include nwk_ip/base/ip_media_if/subdir.mk
-include nwk_ip/app/common/subdir.mk
-include mmcau/mmcau_source/subdir.mk
-include ieee_802.15.4/phy/source/XcvrSPI/subdir.mk
-include ieee_802.15.4/phy/source/MCR20A/MCR20Drv/subdir.mk
-include ieee_802.15.4/phy/source/subdir.mk
-include ieee_802.15.4/App/subdir.mk
-include freertos/portable/subdir.mk
-include freertos/subdir.mk
-include framework/TimersManager/Source/subdir.mk
-include framework/Shell/Source/subdir.mk
-include framework/SerialManager/Source/USB_VirtualCom/Source/subdir.mk
-include framework/SerialManager/Source/SPI_Adapter/subdir.mk
-include framework/SerialManager/Source/subdir.mk
-include framework/SecLib/subdir.mk
-include framework/Reset/Source/subdir.mk
-include framework/RNG/Source/subdir.mk
-include framework/Panic/Source/subdir.mk
-include framework/OSAbstraction/Interface/subdir.mk
-include framework/NVM/Source/subdir.mk
-include framework/Messaging/Source/subdir.mk
-include framework/MemManager/Source/subdir.mk
-include framework/MWSCoexistence/Source/subdir.mk
-include framework/LowPower/Source/subdir.mk
-include framework/Lists/subdir.mk
-include framework/LED/Source/subdir.mk
-include framework/Keyboard/Source/subdir.mk
-include framework/GPIO/Source/subdir.mk
-include framework/FunctionLib/subdir.mk
-include framework/Flash/Internal/subdir.mk
-include framework/FSCI/Source/subdir.mk
-include drivers/subdir.mk
-include board/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
fk64f_KDS_lab_br.srec \


# All Target
all: pre-build main-build

# Main-build Target
main-build: fk64f_KDS_lab_br.elf secondary-outputs

# Tool invocations
fk64f_KDS_lab_br.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross ARM C++ Linker'
	arm-none-eabi-g++ -mcpu=cortex-m4 -mthumb -mfloat-abi=softfp -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -ffreestanding -fno-builtin -Wall -Wno-missing-braces  -g -T ../MK64FN1M0xxx12_connectivity.ld -Xlinker --gc-sections -Wl,-Map,"fk64f_KDS_lab_br.map" --specs=nano.specs -mapcs  -Xlinker -static  -Xlinker -z  -Xlinker muldefs  -Wl,--no-wchar-size-warning -o "fk64f_KDS_lab_br.elf" -Xlinker --start-group $(OBJS) $(USER_OBJS) $(LIBS) -Xlinker --end-group
	@echo 'Finished building target: $@'
	@echo ' '

fk64f_KDS_lab_br.srec: fk64f_KDS_lab_br.elf
	@echo 'Invoking: Cross ARM GNU Create Flash Image'
	arm-none-eabi-objcopy -O srec "fk64f_KDS_lab_br.elf"  "fk64f_KDS_lab_br.srec"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(C++_DEPS)$(OBJS)$(C_DEPS)$(ASM_DEPS)$(CC_DEPS)$(SECONDARY_FLASH)$(CPP_DEPS)$(CXX_DEPS)$(C_UPPER_DEPS)$(S_UPPER_DEPS) fk64f_KDS_lab_br.elf
	-@echo ' '

pre-build:
	-arm-none-eabi-gcc -E -x c -P -Iinclude "C:/Users/ernesto/workspace.kds/fk64f_KDS_lab_br/boards/frdmk64f_frdmcr20a/wireless_examples/thread/border_router/freertos/kds/../../../../../../../middleware/wireless/framework_5.0.5/Common/devices/MK64F12/gcc/MK64FN1M0xxx12_connectivity.lld" -o "C:/Users/ernesto/workspace.kds/fk64f_KDS_lab_br/boards/frdmk64f_frdmcr20a/wireless_examples/thread/border_router/freertos/kds/MK64FN1M0xxx12_connectivity.ld" -DgUseBootloaderLink_d=0 -DgUseInternalStorageLink_d=0 -DgUseNVMLink_d=1 -D__ram_vector_table__=1 -DgNVMSectorCountLink_d=32 -DgEraseNVMLink_d=1
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH)

.PHONY: all clean dependents
.SECONDARY: main-build pre-build

-include ../makefile.targets
