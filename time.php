<?php
// 1. Schakel foutmeldingen uit en set tijdzone
error_reporting(0);
ini_set('display_errors', 0);
date_default_timezone_set('Europe/Amsterdam'); 

// --- BESTANDSLOCATIES ---
$filename = 'current_time.gif'; 
$filepath = __DIR__ . '/' . $filename; 

// --- SYNCHRONISATIE CORRECTIE ---
// Compensatie voor de Strato-serverklok en de vertraging van de Panda.
$offset_seconds = '+7 seconds'; 

// --- DEBUG CODE ---
file_put_contents(__DIR__ . '/cron_log.txt', date("Y-m-d H:i:s") . " - Script gestart.\n", FILE_APPEND);

// --- INTERNE TIMER LOGICA ---
if (file_exists($filepath)) {
    $last_modified = filemtime($filepath);
    $current_time = time();
    $time_since_last_update = $current_time - $last_modified;

    // We stoppen als het bestand minder dan 55 seconden oud is.
    if ($time_since_last_update < 55) {
        file_put_contents(__DIR__ . '/cron_log.txt', date("Y-m-d H:i:s") . " - Gestopt (te vroeg: " . $time_since_last_update . "s).\n", FILE_APPEND);
        exit; 
    }
}
file_put_contents(__DIR__ . '/cron_log.txt', date("Y-m-d H:i:s") . " - **GIF bijwerken.**\n", FILE_APPEND);
// --- EINDE TIMER LOGICA ---


// --- STAP 1: TEKEN OP KLEIN CANVAS (betrouwbare methode) ---
$base_width = 60; // Tijdelijk klein canvas
$base_height = 60; 

$img_base = imagecreate($base_width, $base_height); 
$black = imagecolorallocate($img_base, 0, 0, 0);
$white = imagecolorallocate($img_base, 255, 255, 255);

// VUL ACHTERGROND MET ZWART & MAAK ZWART TRANSPARANT
imagefill($img_base, 0, 0, $black); 
imagecolortransparent($img_base, $black); 

// Bepaal de gecorrigeerde tijdstempel
$time_stamp_corrected = strtotime($offset_seconds); 

// Huidige tijd en datum genereren
$time = date("H:i", $time_stamp_corrected); // Bijv. 07:05
$date = date("j/m/y", $time_stamp_corrected); // Gebruik kleine 'y' voor 25

// --- LETTERGROOTTES DEFINIËREN ---
$font_size_time = 5; 
$font_size_date = 2; 

$font_height_time = imagefontheight($font_size_time); 
$font_height_date = imagefontheight($font_size_date); 

// --- LIJN 1: TIJD (H:i) ---
$text_to_draw_time = $time;
$font_width_time = imagefontwidth($font_size_time) * strlen($text_to_draw_time);

// Positie om ruimte te maken voor de datum eronder
$x_time = floor(($base_width - $font_width_time) / 2); 
$y_time = 18; // Y-positie iets hoger

// Teken de TIJD in WIT
imagestring($img_base, $font_size_time, $x_time, $y_time, $text_to_draw_time, $white); 


// --- LIJN 2: DATUM (j/m/y) ---
$text_to_draw_date = $date;
$font_width_date = imagefontwidth($font_size_date) * strlen($text_to_draw_date);

// Positie voor de datum (onder de tijd)
$x_date = floor(($base_width - $font_width_date) / 2); 
$y_date = $y_time + $font_height_time + 5; // Start na de tijd + 5px extra ruimte

// Teken de DATUM in WIT
imagestring($img_base, $font_size_date, $x_date, $y_date, $text_to_draw_date, $white); 


// --- STAP 2: SCALEN NAAR 240x240 (grote tekst garanderen) ---
$final_width = 240; 
$final_height = 240; 

$img_final = imagecreatetruecolor($final_width, $final_height); // Nieuw canvas
imagefill($img_final, 0, 0, $black); // Vul met ZWART
imagecolortransparent($img_final, $black); // Maak ZWART transparant

// Schaalt het kleine beeld 4x op naar het grotere canvas
imagecopyresampled(
    $img_final, 
    $img_base, 
    0, 0, 0, 0, 
    $final_width, $final_height, 
    $base_width, $base_height    
);

imagedestroy($img_base);

// --- STAP 3: GIF GENEREREN EN PATCHEN ---
$gifData = '';
if (ob_start()) {
    imagegif($img_final);
    $gifData = ob_get_clean();
}
imagedestroy($img_final);

if ($gifData) {
    // --- START PANDA BYPASS PATCH ---
    $bypass_hex = "474946383961F000F000F07371";
    $bypass_binary = pack("H*", $bypass_hex);
    $patched_gifData = substr_replace($gifData, $bypass_binary, 0, 13);
    // --- EINDE PANDA BYPASS PATCH ---

    @chmod($filepath, 0666); 
    file_put_contents($filepath, $patched_gifData);
    file_put_contents(__DIR__ . '/cron_log.txt', date("Y-m-d H:i:s") . " - GIF gepatcht en opgeslagen.\n", FILE_APPEND);
}
// GEEN AFSLUIT-TAG (?>)