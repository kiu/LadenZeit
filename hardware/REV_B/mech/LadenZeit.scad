$fn = $preview?20:360;
//$fn = $preview?20:60;

pcb_thick = 1;

pcb_width = 108;
pcb_height = 34;

pcb_oled_width = 100;
pcb_oled_height = 33.5;

pcb_oled_spacing = 3;

oled_screen_width = 78.8;
oled_screen_height = 21.18;
oled_screen_thick = 2.8;

oled_screen_x_off = 0.75;
oled_screen_y_off = -1.75;

interact_x = 59;

pcb_holes_x = 95;
pcb_holes_y = 28.5;
pcb_holes_dia = 3.4;

// -------------------------------------------------------

c_pcb_width = pcb_width; // +2 not needed, due to circles having higher diameter due to pcb_height
c_pcb_height = pcb_height + 2;

wall = 2;

// STACKUP
//
// Screw Head   3 / 1.4
// PCB plate    1
// Space        6
// PCB main     1
// Space        3
// PCB OLED     1
// OLED         3
// Wall         2


pcb_off = wall + pcb_oled_spacing + pcb_thick + pcb_oled_spacing;
c_thick = 1.4 + pcb_thick + 6 + pcb_thick + pcb_off;

// -------------------------------------------------------

module pcb() {
    difference() {
        union() {            

            // Main PCB
            translate([0, 0, pcb_thick / 2]) cube ([pcb_width, pcb_height, pcb_thick], center = true);            
            // Left half
            translate([-pcb_width / 2, 0, pcb_thick / 2]) cylinder(h=pcb_thick, d=pcb_height, center = true);            
            // Right half
            translate([ pcb_width / 2, 0, pcb_thick / 2]) cylinder(h=pcb_thick, d=pcb_height, center = true);
            
            // Oled PCB            
            translate([0, 0, pcb_thick + (pcb_oled_spacing / 2 + pcb_thick / 2)]) cube ([pcb_oled_width, pcb_oled_height, pcb_oled_spacing + pcb_thick], center = true);
            
            // Oled Screen
            translate([oled_screen_x_off, oled_screen_y_off, pcb_thick + pcb_oled_spacing + pcb_thick + oled_screen_thick / 2]) cube ([oled_screen_width, oled_screen_height, oled_screen_thick], center = true);
            // Power
            translate([-interact_x, 0, pcb_thick + 3]) cylinder(h=6, d=8, center = true);
            translate([-interact_x, 0, pcb_thick + 6 + 2.5]) cylinder(h=5, d=11, center = true);
            
            // Rotary
            translate([interact_x, 0, pcb_thick + 3]) cube ([12, 14, 6], center = true);
            translate([interact_x, 0, pcb_thick + 10.5]) cylinder(h=21, d=7, center = true);
            translate([interact_x, 0, pcb_thick + 15]) cylinder(h=16, d=17, center = true);
            
            // Plate PCB
            translate([0, 0, pcb_thick / 2 - 9 - pcb_thick]) cube ([pcb_width, pcb_height, pcb_thick], center = true);
            // Left half
            translate([-pcb_width / 2, 0, pcb_thick / 2 - 9 - pcb_thick]) cylinder(h=pcb_thick, d=pcb_height, center = true);            
            // Right half
            translate([ pcb_width / 2, 0, pcb_thick / 2 - 9 - pcb_thick]) cylinder(h=pcb_thick, d=pcb_height, center = true);
        }
        
        // Main Holes
        h = pcb_thick + pcb_oled_spacing + pcb_thick;
        translate([-pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, h / 2]) cylinder(h=h, d=pcb_holes_dia, center = true);
        translate([-pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, h / 2]) cylinder(h=h, d=pcb_holes_dia, center = true);
        translate([ pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, h / 2]) cylinder(h=h, d=pcb_holes_dia, center = true);
        translate([ pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, h / 2]) cylinder(h=h, d=pcb_holes_dia, center = true);

        // Plate Holes
        hbot = pcb_thick + 9 + pcb_thick;
        translate([-pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, -hbot / 2]) cylinder(h=hbot, d=pcb_holes_dia, center = true);
        translate([-pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, -hbot / 2]) cylinder(h=hbot, d=pcb_holes_dia, center = true);
        translate([ pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, -hbot / 2]) cylinder(h=hbot, d=pcb_holes_dia, center = true);
        translate([ pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, -hbot / 2]) cylinder(h=hbot, d=pcb_holes_dia, center = true);
    }
}

module case() {
    height_w2 = wall + c_pcb_height + wall;
    
    difference() {
        union() {            
            // Main PCB
            translate([0, 0, 0]) cube ([c_pcb_width, height_w2, c_thick]);            
            // Left half
            translate([0           , (height_w2)/ 2, 0]) cylinder(h=c_thick, d=height_w2);
            // Right half
            translate([ c_pcb_width, (height_w2)/ 2, 0]) cylinder(h=c_thick, d=height_w2);
        }
  
        translate([c_pcb_width / 2, height_w2/ 2, 0]) {
            // Rotary
            translate([interact_x, 0, 0]) cylinder(h=c_thick, d=19);  
            translate([interact_x - 13/2, -16/2, 2]) cube ([13, 16, 7]);
            
            // Power
            translate([-interact_x, 0, 0]) cylinder(h=pcb_off, d=10);            
            translate([-interact_x, 0, 0]) cylinder(h=7, d=13);
            translate([-interact_x + 7/2 - 100, -height_w2 / 2, 0]) cube ([100, height_w2, 4]);
                       
            // Main Holes            
            translate([-pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, 0]) cylinder(h=c_thick, d=pcb_holes_dia);
            translate([-pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, 0]) cylinder(h=c_thick, d=pcb_holes_dia);
            translate([ pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, 0]) cylinder(h=c_thick, d=pcb_holes_dia);
            translate([ pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, 0]) cylinder(h=c_thick, d=pcb_holes_dia);

            // Main Head // 6.4
            translate([-pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, 0]) cylinder(h=1.2, d=7);
            translate([-pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, 0]) cylinder(h=1.2, d=7);
            translate([ pcb_holes_x / 2 + oled_screen_x_off, -pcb_holes_y / 2, 0]) cylinder(h=1.2, d=7);
            translate([ pcb_holes_x / 2 + oled_screen_x_off,  pcb_holes_y / 2, 0]) cylinder(h=1.2, d=7);

            // OLED Window
            translate([-oled_screen_width / 2 + oled_screen_x_off, -oled_screen_height / 2 - oled_screen_y_off, 0]) cube ([oled_screen_width, oled_screen_height, c_thick]);
            // OLED
            translate([-91.5 / 2 + oled_screen_x_off, -33 / 2, wall]) cube ([91.5, 33, pcb_oled_spacing]);            
            // OLED connector
            translate([-7 / 2 - pcb_holes_x / 2, -21 / 2, wall]) cube ([7, 21, pcb_oled_spacing]);
            // OLED PCB
            translate([-(1 + pcb_oled_width + 1) / 2 + 0.75, -c_pcb_height / 2, wall + pcb_oled_spacing]) cube ([1 + pcb_oled_width + 1, c_pcb_height, c_thick]);
           
            // MAIN PCB
            translate([-c_pcb_width / 2, -c_pcb_height / 2, pcb_off]) cube ([c_pcb_width, c_pcb_height, c_thick]);
            // Left half
            translate([-c_pcb_width / 2, 0, pcb_off]) cylinder(h=c_thick, d=c_pcb_height);
            // Right half
            translate([ c_pcb_width / 2, 0, pcb_off]) cylinder(h=c_thick, d=c_pcb_height);            
        }
   }
}

color([0,0,1]) case();

//color([1,0,0]) translate([c_pcb_width / 2, (wall + c_pcb_height + wall)/ 2, c_thick/2 + 2 - 3]) rotate ([180, 0, 0]) pcb();
