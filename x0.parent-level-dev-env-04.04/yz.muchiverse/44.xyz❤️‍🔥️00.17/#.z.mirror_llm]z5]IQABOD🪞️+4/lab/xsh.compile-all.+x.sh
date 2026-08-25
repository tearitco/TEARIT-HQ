#!/bin/bash

# Define the output directory
output_dir="+x"
button_dir="./"

# Create the +x/ directory if it doesn't exist
if [ ! -d "$output_dir" ]; then
    mkdir "$output_dir"
    if [ $? -ne 0 ]; then
        echo "Error: Could not create directory $output_dir"
        exit 1
    fi
fi

# Compile any .c files in the current directory
for file in *.c; do
    # Check if any .c files exist (if no matches, *.c becomes literal)
    if [ -f "$file" ]; then
        # Extract the filename without the .c extension
        basename=${file%.c}

        # Append .+x to the executable name
        executable_name="${basename}.+x"

        # Compile the .c file into an executable in the +x/ directory with .+x suffix
        gcc "$file" -o "$output_dir/$executable_name" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lGLEW -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -lassimp -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL

        # Check the exit status of gcc
        if [ $? -eq 0 ]; then
            echo "Successfully compiled $file into $output_dir/$executable_name"
        else
            echo "Error compiling $file"
        fi
    fi
done

# Also compile modules in the modules/ directory (both direct files and subdirectories)
for module_c in modules/*.c; do
    if [ -f "$module_c" ]; then
        module_file=$(basename "$module_c" .c)
        module_executable="modules/+x/${module_file}.+x"
        
        mkdir -p "modules/+x"
        
        gcc "$module_c" -o "$module_executable" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lGLEW -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -lassimp -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL
        
        if [ $? -eq 0 ]; then
            echo "Successfully compiled $module_c into $module_executable"
        else
            echo "Error compiling $module_c"
        fi
    fi
done

# Also compile modules in subdirectories of modules/
for dir in modules/*; do
    if [ -d "$dir" ]; then
        for module_c in "$dir"/*.c; do
            if [ -f "$module_c" ]; then
                module_dir=$(basename "$dir")
                module_file=$(basename "$module_c" .c)
                module_executable="modules/+x/${module_file}.+x"
                
                mkdir -p "modules/+x"
                
                gcc "$module_c" -o "$module_executable" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lGLEW -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -lassimp -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL
                
                if [ $? -eq 0 ]; then
                    echo "Successfully compiled $module_c into $module_executable"
                else
                    echo "Error compiling $module_c"
                fi
            fi
        done
    fi
done

echo "Compilation complete."
