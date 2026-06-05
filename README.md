Command:  

cmake -S . -B build -G "MinGW Makefiles"  
cmake --build build  

On Ubuntu:  

cmake -S . -B build -G "Unix Makefiles"  
cmake --build build  

In school computer :

source /home/gurousta/vulkan/1.4.350.1/setup-env.sh  

cmake -S . -B build \  
  -DVulkan_INCLUDE_DIR="$VULKAN_SDK/include" \  
  -DVulkan_LIBRARY="$VULKAN_SDK/lib/VulkanLoader/lib/libvulkan.so.1.4.350"  

cmake --build build-vulkan14  