################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../AddressPFS.c \
../Cache.c \
../Console.c \
../Disk.c \
../FAT.c \
../FUSE.c \
../PFS.c \
../PFSPrint.c 

OBJS += \
./AddressPFS.o \
./Cache.o \
./Console.o \
./Disk.o \
./FAT.o \
./FUSE.o \
./PFS.o \
./PFSPrint.o 

C_DEPS += \
./AddressPFS.d \
./Cache.d \
./Console.d \
./Disk.d \
./FAT.d \
./FUSE.d \
./PFS.d \
./PFSPrint.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D_FILE_OFFSET_BITS=64 -I"/home/utn_so/Desarrollo/Workspace/Commons" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


