################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/finalPrep.cpp \
../src/posixThread.cpp 

OBJS += \
./src/finalPrep.o \
./src/posixThread.o 

CPP_DEPS += \
./src/finalPrep.d \
./src/posixThread.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -I"/home/osboxes/ensc351_proj/Part5/Ensc351Part2SolnLib" -I"/home/osboxes/ensc351_proj/Part5/Ensc351" -O0 -g3 -fsanitize=address       -fsanitize=leak -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


