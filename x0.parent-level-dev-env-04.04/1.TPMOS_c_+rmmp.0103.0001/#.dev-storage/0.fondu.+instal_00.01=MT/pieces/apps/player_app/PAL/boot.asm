# boot.asm - PLAYR Bootstrap Script
# Responsibility: Main menu loop and project loading dispatch.

start:
    # 1. Show Home Menu
    menu_op pieces/apps/player_app/PAL/home_menu.pdl
    
    # 2. Check Selection
    li r1, 1
    beq r0, r1, run_loader
    
    li r1, 4
    beq r0, r1, exit_app
    
    j start

run_loader:
    # 3. Project Selection
    # project_loader returns index > 0 if success
    project_loader
    
    # Check if a project was selected
    li r1, 0
    beq r0, r1, start # Go back if cancelled
    
    # 4. Project Boot (Title Screen)
    # Once project_id is in state.txt, we can run project scripts
    console_print "Project Loaded! Transitioning to Title Screen..."
    halt

exit_app:
    console_print "Goodbye!"
    halt
