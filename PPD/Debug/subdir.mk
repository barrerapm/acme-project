################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../PPD.c \
../algoritmos.c 

OBJS += \
./PPD.o \
./algoritmos.o 

C_DEPS += \
./PPD.d \
./algoritmos.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D__USE_LARGEFILE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I"/home/utn_so/Desarrollo/Workspace/Commons" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


