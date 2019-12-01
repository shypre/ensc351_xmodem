################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/TestCircularBuffer.cpp 

OBJS += \
./src/TestCircularBuffer.o 

CPP_DEPS += \
./src/TestCircularBuffer.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(CXX) $(TIMESTORM_COMPILER_FLAGS) "$(@:%.o=%.d)" -MT "$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


