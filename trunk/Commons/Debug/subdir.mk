################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Addressing.c \
../Aux.c \
../Pack.c \
../Socket.c \
../Type.c \
../list.c \
../log.c \
../queue.c \
../utils.c 

OBJS += \
./Addressing.o \
./Aux.o \
./Pack.o \
./Socket.o \
./Type.o \
./list.o \
./log.o \
./queue.o \
./utils.o 

C_DEPS += \
./Addressing.d \
./Aux.d \
./Pack.d \
./Socket.d \
./Type.d \
./list.d \
./log.d \
./queue.d \
./utils.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


