// [main.c]
// this template is provided for the 2D shooter game.

#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <math.h>

// If defined, logs will be shown on console and written to file.
// If commented out, logs will not be shown nor be saved.
//#define LOG_ENABLED

/* Constants. */

// Mathematical Constants
#define PI 3.14159265

// Frame rate (frame per second)
const int FPS = 60;
// Display (screen) width.
const int SCREEN_W = 800;
// Display (screen) height.
const int SCREEN_H = 800;
// At most 32 audios can be played at a time.
const int RESERVE_SAMPLES = 256;
// At most 10 transition segments can be lined up at a time.
#define TRANSITION_SEGS 10
// Same as:
// const int SCENE_MENU = 1;
// const int SCENE_START = 2;
enum {
	SCENE_MENU = 1,
	SCENE_START = 2,

	SCENE_SETTINGS = 3,

	SCENE_GAME_OVER = 4,
	SCENE_CUTSCENE = 5, // Unused
	SCENE_RESULTS = 6,
	SCENE_TRANSITION = 7,
	SCENE_TITLECARD = 8,
	SCENE_EXIT = 9
};

/* Input states */

// The active scene id.
int active_scene;
// Keyboard state, whether the key is down or not.
bool key_state[ALLEGRO_KEY_MAX];
// Mouse state, whether the key is down or not.
// 1 is for left, 2 is for right, 3 is for middle.
bool *mouse_state;
// Mouse position.
int mouse_x, mouse_y;
// TODO: More variables to store input states such as joysticks, ...
bool game_paused = false;


/* Variables for allegro basic routines. */

ALLEGRO_DISPLAY* game_display;
ALLEGRO_EVENT_QUEUE* game_event_queue;
ALLEGRO_TIMER* game_update_timer;

/* Shared resources*/

ALLEGRO_FONT* font_OpenSans_32;
ALLEGRO_FONT* font_OpenSans_24;
ALLEGRO_FONT* font_pirulen_32;
ALLEGRO_FONT* font_pirulen_24;
ALLEGRO_FONT* font_advanced_pixel_lcd_10;
ALLEGRO_FONT* font_impacted;
ALLEGRO_SAMPLE* button_click_sfx;
ALLEGRO_SAMPLE_ID button_click_sfx_id;
// TODO: More shared resources or data that needed to be accessed
// across different scenes.

// For game_draw(): If true, gradually decrease a given color hue over the screen until transparent. Becomes false after completion.
bool fade_in = false;
// For game_draw(): If true, gradually increase a given color hue over the screen until fully-colored. Becomes false after completion. 
bool fade_out = false;

// Fade parameters

unsigned char fade_r;
unsigned char fade_g;
unsigned char fade_b;
unsigned char fade_current_alpha;
unsigned char fade_target_alpha;
unsigned char fade_rate;

/* Transition resources*/

// The target scene of the transition. (Refer to enum of scenes)
int transition_target;

// The mode of the transition. (TODO)
// List of modes:
// - 'F': Fade
//unsigned char transition_mode;


/* Menu Scene resources*/

/* ANIMATION_SHEET: Load a image to the bitmap, specify the
number of rows and columns of cells. (Cell width and height
can be inferred) With the current cell's location specified,
and the cell width and height given, the region to draw from
the bitmap can be obtained.*/
typedef struct {
	// Must be segmented into rectangular cells.
	ALLEGRO_BITMAP* sheet;
	float cell_width;
	float cell_height;
	int cell_rows;
	int cell_columns;
	int current_row;
	int current_column;
	// Default = cell_columns - 1. Necessary since the last row might not be completely filled.
	int last_col;
	// Starts from 0.
	int frame_repeat;
	// Should default to 0 (no dupe frames). 
	int dupe_frames;
} ANIMATION_SHEET;



ANIMATION_SHEET main_img_background_anim;

ALLEGRO_BITMAP* img_settings;
ALLEGRO_BITMAP* img_settings2;
ALLEGRO_SAMPLE* main_bgm;
ALLEGRO_SAMPLE_ID main_bgm_id;

/* Start Scene resources*/
ALLEGRO_BITMAP* start_img_background;
ALLEGRO_BITMAP* cloud_layer_1;
ALLEGRO_BITMAP* cloud_layer_2;
ALLEGRO_BITMAP* cloud_layer_3;
unsigned char start_bg_alpha;
float bg_pos_y; // Position of background image, for scrolling.
float layer_1_pos_y;
float layer_2_pos_y;
float layer_3_pos_y;
float scroll_speed = 10; // Speed of scrolling.
double transition_timestamp; // General timestamp for transition events.

ALLEGRO_TIMER* ingame_timer; // Timer that keeps track of time in the level.
double pause_time_record; // Records time when paused.
int score = 0; // Records current score.
int lives; // Records current lives.
int combo = 1; // Records current combo streak. Resets to 1 when taking damage or when an enemy despawns naturally (Miss).
float sync_percentage = 0; // Records current Sync percentage. Between 0% and 100%.
double last_unsync_timestamp = 0;
double MIN_SYNC_TIME = 5.0f;
float sync_rate = 0;
bool is_synchronizing = false;
bool is_octa_dormant = true;
double last_octa_switch_timestamp = 0;
double OCTA_SWITCH_COOLDOWN = 1.0f;

int start_scene_phase;

	/* - UI Elements - */
// TODO: Prepare UI elements and load them in game_init().

ALLEGRO_BITMAP* player_UI_container; // The box that holds UI elements related to the player. Purely visual.
// This box should have: A box for score display, two boxes for meters, and a section for lives.

// Load player UI's animated elements according to current variables.
void draw_player_UI_elements();

ALLEGRO_BITMAP* boss_UI_container; // The box that holds UI elements related to the boss. Purely visual.



	/* - Level Scripting - */
FILE *script; // Script for the level's enemy spawning.
bool script_done = false; // Boolean to keep track of the script's status.
/*
	- - FORMAT DOCUMENTATION OF *script - - 
	Every set of inputs contains three lines:
	Line 1: A double, the timestamp of the event.
	Line 2: An integer, the number of spawn instances.
	Line 3: Sprite (Animation) id.
	Line 4: HP.
	Line 5: Attack Mode and Movement Mode. (char)

	Attack Modes:
	'A' - Aimed Rapid Fire
	'B' - 3x Scatter
	'C' - 5x Scatter
	'D' - 5x Circle Shot

	And then depending on the Movement Mode...

	'A' - Straight line: Start(x, y), Velocity(x, y), Acceleration(x, y)
	'B' - Homing: Start(x, y), Velocity(x, y), Acceleration(x, y)
	'C' - Fixed Vector: Start(x, y), Destination(x, y, as vectors), Frames (int)
	'D' - Sinusoidal: Start(x, y), Velocity(x, y), Accel Amplitude(x, y)
	'E' - Ricochet: Start(x, y), Velocity(x, y), Acceleration(x, y)
	
	
*/

double next_event_timestamp; // The timestamp of the next event on the script.


	/* - Dialogue Scripting - */
// Note: Cut due to time restraints. (TODO?)
FILE *dialogue; // Script for dialogues.
/*
	- - FORMAT DOCUMENTATION FOR *dialogue - -
	Every input follows the following format:
	- Line 1: Event Type (char)
		- 'D' - Dialogue
		- 'P' - Prompt (Popup)
		- 'M' - (Character) Movement
		- 'T' - Toggle Hidden (Change the object's hidden state)
		- 'S' - Sound Effect
	- Line 2:
		- Dialogue: The Speaker's name. (string)
		- Prompt: The prompt's content. (string)
		- Movement: The filename of the image to move. (string)
		- Toggle Hidden: The filename of the image to toggle hidden. (string)
		- Sound Effect: The name of the sound effect. (string)
	- Line 3:
		- Dialogue: The text to be displayed. (string)
		- Prompt: (Optional) The filename of the image to display. (string)
		- Movement:
			- If the image to move has already been loaded: The destination's X and Y position. (float)
			- Otherwise: The source and destination's X and Y position. (float)
	
	Input is terminated upon EOF.
*/

bool next_dialogue; // True: Read the next dialogue event. False: Wait until user hits Enter.

ALLEGRO_BITMAP* start_img_player;
ALLEGRO_BITMAP* player_animation_default;
ALLEGRO_BITMAP* img_octa;
ALLEGRO_BITMAP* enemy_animation_default;
ANIMATION_SHEET enemy_animation_array[10];
ALLEGRO_SAMPLE* enemy_death_sfx;
ALLEGRO_SAMPLE_ID enemy_death_sfx_id;

ALLEGRO_SAMPLE* start_bgm;
ALLEGRO_SAMPLE_ID start_bgm_id;

ALLEGRO_BITMAP* img_bullet;
ALLEGRO_SAMPLE* player_laser_sfx;
ALLEGRO_SAMPLE_ID player_laser_sfx_id;
ALLEGRO_SAMPLE* upgrade_sfx;
ALLEGRO_SAMPLE_ID upgrade_sfx_id;

ALLEGRO_BITMAP* octa_charge_shot;
ALLEGRO_SAMPLE* octa_charge_shot_sfx;
ALLEGRO_SAMPLE_ID octa_charge_shot_sfx_id;
ALLEGRO_BITMAP* enemy_bullet_1;

ALLEGRO_SAMPLE* small_explosion_sfx;
ALLEGRO_SAMPLE_ID small_explosion_sfx_id;

ALLEGRO_BITMAP* img_boss;
ALLEGRO_BITMAP* img_boss_overdrive;
ALLEGRO_BITMAP* img_lightning;
ALLEGRO_BITMAP* img_warning_label;
ALLEGRO_SAMPLE* thunder_sfx;
ALLEGRO_SAMPLE_ID thunder_sfx_id;
ALLEGRO_SAMPLE* boss_bgm_A;
ALLEGRO_SAMPLE* boss_bgm_B;
ALLEGRO_SAMPLE_ID boss_bgm_A_id;
ALLEGRO_SAMPLE_ID boss_bgm_B_id;

ALLEGRO_SAMPLE* warning_sfx;
ALLEGRO_SAMPLE_ID warning_sfx_id;

typedef struct {
	// The center coordinate of the image.
	float x, y;
	// The width and height of the object.
	float w, h;
	// The velocity in x, y axes.
	float vx, vy;
	// The acceleration in x, y axes.
	float ax, ay;
	// The base damage of the object upon collision. (Applicable to bullets and enemies)
	float base_damage;
	// The current HP of the object. (Applicable to player and enemies)
	float current_HP;
	// The maximum HP of the object. (Applicable to player and enemies)
	float max_HP;
	// Radius of hitbox, starting from (x, y).
	float hitbox_radius;
	// The hitbox's source's X position relative to the image's center.
	float hitbox_source_x_offset;
	// The hitbox's source's Y position relative to the image's center.
	float hitbox_source_y_offset;
	// Should we draw this object on the screen.
	bool hidden;
	// Does the object hurt the player on collision? (antonym: friendly)
	// Note: Friendly Bullet vs Enemy = Enemy takes damage || Hostile Bullet/Enemy vs Player = Player takes damage.
	bool hostile;
	// Attack Mode: Enemy-Only, used to determine firing pattern.
	char attack_mode;
	// Firing Cooldown (for enemies)
	float firing_cooldown;
	// Last Shoot Timestamp (For enemies)
	float last_shoot_timestamp;
	// Movement Mode: Enemy-Only, used to determine movement pattern.
	char movement_mode;
	// Timestamp of spawn. Used in game_update for easy tracking of enemy AI progress.
	double spawn_timestamp; // TODO: MAKE THIS WORK
	// The pointer to the object’s image.
	ALLEGRO_BITMAP* img;
	// The object's animation sheets, if applicable.
	ANIMATION_SHEET animations[10];
	// The object's transparency.
	unsigned char alpha;
	// id: Enemy-Only, used to determine the enemy type for special interactions.
	int id;
} MovableObject;

typedef struct {
	float x;
	float y;
} Point;

void draw_movable_object(MovableObject obj, int flag, int anim_id);

// Draw circle hitbox for movable objects.
void draw_circle_hitbox(MovableObject obj, float radius);

// Test for collision between two circle hitboxes. Returns true if there is collision.
bool circle_collision_test(MovableObject obj1, MovableObject obj2);

bool circle_line_collision_test(MovableObject obj, float x1, float y1, float x2, float y2);

// Given dx and dy, return the unit vector split into x and y directions.
void unit_vector(float dx, float dy, float *vx, float *vy);

// Given vx and vy, rotate the unit vector counter-clockwise by theta. (in degrees)
void rotate_vector(float* vx, float* vy, double theta);

// Given two points (x1, y1), (x2, y2), as well as a distance (d), construct a parallelogram by drawing two triangles.
// Flags:
//		0 - Hollow
//      1 - Filled
void draw_parallelogram(float x1, float y1, float x2, float y2, float d, ALLEGRO_COLOR color, int flag);

void draw_dotted_line(float x1, float y1, float x2, float y2, ALLEGRO_COLOR color);

// Given a fixed coordinate, thickness, and color, as well as the address of the alpha,
// draw a bigger circle every frame until the alpha becomes 0/255 depending on the rate given.
// If thickness is 0, then the circle is filled. (ie. expanding circular shadow)
// A boolean is passed for draw and update to keep track of the fading state.
// Another boolean to determine if it's expanding or shrinking.
void draw_expanding_fading_circle(float x, float y, float thickness, float r, float g, float b, float* alpha, float rate, bool *fading, bool expanding);

#define MAX_ENEMY 64
#define MAX_BULLET 6400
#define MAX_POINT_STACK 100
int active_enemy_count;
bool transition[TRANSITION_SEGS];
MovableObject player;
// Player's movement speed can be modified. Needs to be initialized.
float player_move_speed;
// Is player in focus mode?
bool in_focus = false;
// Is player still invulnerable?
bool player_invulnerable = false;

// The player's damage per bullet.
float player_damage = 10;
// The player's current attack level. Can be upgraded.
float player_attack_level;
// The player's I-time (damage cooldown).
float MAX_DAMAGE_COOLDOWN = 0.5f;
float respawn_x;
float respawn_y;
bool dying;
float dying_alpha;
bool respawning;
float respawning_alpha;
ALLEGRO_SAMPLE* player_damaged_sfx;
ALLEGRO_SAMPLE_ID player_damaged_sfx_id;
ALLEGRO_SAMPLE* death_sfx;
ALLEGRO_SAMPLE_ID death_sfx_id;

double last_damage_timestamp = 0;

// Set up variables related to death when needed.
void death_routine();

MovableObject boss;
bool boss_damaged_by_ult;

MovableObject octa;


MovableObject enemies[MAX_ENEMY];
MovableObject bullets[MAX_BULLET];

// Variables that control "Thunder" spells.
Point point_stack[MAX_POINT_STACK];
int point_stack_counter;
int thunder_cap;
bool release_lightning;

// Variables that control "Laser Array" spells.
typedef struct {
	bool is_active;
	bool vertical;
	bool horizontal;
	bool mirror;
	bool limit_reached;
	float x;
	float y;
	float v;
	float v_limit;
	float a;
	float gap_halfwidth;
	float vert_gap_pos;
	float vert_gap_pos_mirror;
	float hori_gap_pos;
	float hori_gap_pos_mirror;
} LaserArray;
LaserArray laser_array;
ALLEGRO_BITMAP* img_laser_array;
ALLEGRO_BITMAP* img_laser_array_horizontal;
ALLEGRO_SAMPLE* laser_array_sfx;
ALLEGRO_SAMPLE_ID laser_array_sfx_id;

void laser_array_routine(double now);

// Set up bullet shooting cool-down variables.

const float MAX_COOLDOWN = 0.2f;
double last_shoot_timestamp = 0;
const float MIN_CHARGE_TIME = 1.0f;
double last_release_charge_timestamp = 0;
int player_bullet_speed = -8; // Player's bullet speed can be modified. Initialized as -8.

// Ult variables.
bool ult_active;
double ult_trigger_timestamp;
MovableObject ult_hitbox;
ALLEGRO_SAMPLE* ult_sfx_A;
ALLEGRO_SAMPLE* ult_sfx_B;
ALLEGRO_SAMPLE_ID ult_sfx_A_id;
ALLEGRO_SAMPLE_ID ult_sfx_B_id;

/* Results Scene resources*/
ALLEGRO_BITMAP* results_img_background;
ALLEGRO_SAMPLE* results_bgm;
ALLEGRO_SAMPLE_ID results_bgm_id;
FILE* scoreboard;
int score_list[11];
int compare_int(const void* a, const void* b)
{
	return -(*(int*)a - *(int*)b);
}

/* Game Over Scene resources */

ALLEGRO_BITMAP* game_over_img_background;
ALLEGRO_SAMPLE* game_over_bgm;
ALLEGRO_SAMPLE_ID game_over_bgm_id;

/* Titlecard Resources*/
ALLEGRO_BITMAP* img_titlecard;

/* Declare function prototypes. */

// Initialize allegro5 library
void allegro5_init(void);
// Initialize variables and resources.
// Allows the game to perform any initialization it needs before
// starting to run.
void game_init(void);
// Process events inside the event queue using an infinity loop.
void game_start_event_loop(void);
// Run game logic such as updating the world, checking for collision,
// switching scenes and so on.
// This is called when the game should update its logic.
void game_update(void);
// Draw to display.
// This is called when the game should draw itself.
void game_draw(void);
// Release resources.
// Free the pointers we allocated.
void game_destroy(void);
// Function to change from one scene to another.
void game_change_scene(int next_scene);
// Load resized bitmap and check if failed.
ALLEGRO_BITMAP *load_bitmap_resized(const char *filename, int w, int h);
// Determines whether the point (px, py) is in rect (x, y, w, h).
bool pnt_in_rect(int px, int py, int x, int y, int w, int h);
// Draw animation from an animation sheet.
void draw_animation(ANIMATION_SHEET anim, float dx, float dy, int flag);


/* Event callbacks. */
void on_key_down(int keycode);
void on_key_up(int keycode);
void on_mouse_down(int btn, int x, int y);

/* Declare function prototypes for debugging. */

// Display error message and exit the program, used like 'printf'.
// Write formatted output to stdout and file from the format string.
// If the program crashes unexpectedly, you can inspect "log.txt" for
// further information.
void game_abort(const char* format, ...);
// Log events for later debugging, used like 'printf'.
// Write formatted output to stdout and file from the format string.
// You can inspect "log.txt" for logs in the last run.
void game_log(const char* format, ...);
// Log using va_list.
void game_vlog(const char* format, va_list arg);

int main(int argc, char** argv) {
	// Set random seed for better random outcome.
	srand(time(NULL));
	allegro5_init();
	game_log("Allegro5 initialized");
	game_log("Game begin");
	// Initialize game variables.
	game_init();
	game_log("Game initialized");
	// Draw the first frame.
	game_draw();
	game_log("Game start event loop");
	// This call blocks until the game is finished.
	game_start_event_loop();
	game_log("Game end");
	game_destroy();
	return 0;
}

void allegro5_init(void) {
	if (!al_init())
		game_abort("failed to initialize allegro");

	// Initialize add-ons.
	if (!al_init_primitives_addon())
		game_abort("failed to initialize primitives add-on");
	if (!al_init_font_addon())
		game_abort("failed to initialize font add-on");
	if (!al_init_ttf_addon())
		game_abort("failed to initialize ttf add-on");
	if (!al_init_image_addon())
		game_abort("failed to initialize image add-on");
	if (!al_install_audio())
		game_abort("failed to initialize audio add-on");
	if (!al_init_acodec_addon())
		game_abort("failed to initialize audio codec add-on");
	if (!al_reserve_samples(RESERVE_SAMPLES))
		game_abort("failed to reserve samples");
	if (!al_install_keyboard())
		game_abort("failed to install keyboard");
	if (!al_install_mouse())
		game_abort("failed to install mouse");
	// TODO: Initialize other addons such as video, ...

	// Setup game display.
	game_display = al_create_display(SCREEN_W, SCREEN_H);
	if (!game_display)
		game_abort("failed to create display");
	al_set_window_title(game_display, "I2P(I)_2019 Final Project 108062101");

	// Setup update timer.
	game_update_timer = al_create_timer(1.0f / FPS);
	if (!game_update_timer)
		game_abort("failed to create update timer");

	// Setup event queue.
	game_event_queue = al_create_event_queue();
	if (!game_event_queue)
		game_abort("failed to create event queue");

	// Malloc mouse buttons state according to button counts.
	const unsigned m_buttons = al_get_mouse_num_buttons();
	game_log("There are total %u supported mouse buttons", m_buttons);
	// mouse_state[0] will not be used.
	mouse_state = malloc((m_buttons + 1) * sizeof(bool));
	memset(mouse_state, false, (m_buttons + 1) * sizeof(bool));

	// Register display, timer, keyboard, mouse events to the event queue.
	al_register_event_source(game_event_queue, al_get_display_event_source(game_display));
	al_register_event_source(game_event_queue, al_get_timer_event_source(game_update_timer));
	al_register_event_source(game_event_queue, al_get_keyboard_event_source());
	al_register_event_source(game_event_queue, al_get_mouse_event_source());
	// TODO: Register other event sources such as timer, video, ...

	// Start the timer to update and draw the game.
	al_start_timer(game_update_timer);
}

void game_init(void) {
	/* Shared resources*/
	font_OpenSans_32 = al_load_ttf_font("OpenSans-Bold.ttf", 32, 0);
	if (!font_OpenSans_32)
		game_abort("failed to load font: OpenSans.ttf with size 32");

	font_OpenSans_24 = al_load_ttf_font("OpenSans-Bold.ttf", 24, 0);
	if (!font_OpenSans_24)
		game_abort("failed to load font: OpenSans.ttf with size 24");

	font_pirulen_32 = al_load_ttf_font("pirulen.ttf", 32, 0);
	if (!font_pirulen_32)
		game_abort("failed to load font: pirulen.ttf with size 32");

	font_pirulen_24= al_load_ttf_font("pirulen.ttf", 24, 0);
	if (!font_pirulen_24)
		game_abort("failed to load font: pirulen.ttf with size 24");

	font_advanced_pixel_lcd_10 = al_load_ttf_font("advanced_pixel_lcd-7.ttf", 10, 0);
	if (!font_advanced_pixel_lcd_10)
		game_abort("failed to load font: advanced_pixel_lcd-7.ttf");

	font_impacted = al_load_ttf_font("impacted.ttf", 100, 0);

	button_click_sfx = al_load_sample("Click_Electronic_12.ogg");
	if (!button_click_sfx)
		game_abort("failed to load button click sfx");

	/* Menu Scene resources*/
	main_img_background_anim.sheet = al_load_bitmap("animated_main_bg.png");
	if (!main_img_background_anim.sheet)
		game_abort("failed to load animated main bg sheet");
	main_img_background_anim.cell_columns = 3;
	main_img_background_anim.cell_rows = 1;
	main_img_background_anim.cell_height = 800;
	main_img_background_anim.cell_width = 800;
	main_img_background_anim.current_column = 0;
	main_img_background_anim.current_row = 0;
	main_img_background_anim.dupe_frames = 2;
	main_img_background_anim.frame_repeat = 0;
	main_img_background_anim.last_col = 3;

	main_bgm = al_load_sample("UraomotE.ogg");
	if (!main_bgm)
		game_abort("failed to load audio: UraomotE.ogg");

	img_settings = al_load_bitmap("settings.png");
	if (!img_settings)
		game_abort("failed to load image: settings.png");
	img_settings2 = al_load_bitmap("settings2.png");
	if (!img_settings2)
		game_abort("failed to load image: settings2.png");

	/* Start Scene resources*/
	start_img_background = load_bitmap_resized("sea_scroll_bg.png", SCREEN_W, SCREEN_H);
	cloud_layer_1 = load_bitmap_resized("cloud_scroll_layer_1.png", SCREEN_W, SCREEN_H);
	cloud_layer_2 = load_bitmap_resized("cloud_scroll_layer_2.png", SCREEN_W, SCREEN_H);
	cloud_layer_3 = load_bitmap_resized("cloud_scroll_layer_3.png", SCREEN_W, SCREEN_H);


	start_img_player = load_bitmap_resized("protag_base.png", 192, 192);
	player_animation_default = load_bitmap_resized("protag_wings.png", 1152, 192);
	player_damaged_sfx = al_load_sample("taking_damage_sfx.ogg");
	if (!player_damaged_sfx)
		game_abort("failed to load player damaged sfx");
	death_sfx = al_load_sample("death_sfx.ogg");
	if (!death_sfx)
		game_abort("failed to load death sfx");

	img_octa = load_bitmap_resized("Octa.png", 192, 192);

	img_boss = load_bitmap_resized("antag_base.png", 192, 192);
	img_boss_overdrive = load_bitmap_resized("antag_overdrive.png", 192, 192);
	img_lightning = load_bitmap_resized("lightning_bolt.png", 150, 600);
	img_warning_label = al_load_bitmap("warning_label.png");
	if (!img_warning_label)
		game_abort("failed to load image: warning_label.png");
	thunder_sfx = al_load_sample("thunder_sfx.ogg");
	if (!thunder_sfx)
		game_abort("failed to load thunder sfx");
		
	player_UI_container = al_load_bitmap("playerUI.png");
	if (!player_UI_container)
		game_abort("failed to load image: playerUI.png");

	boss_UI_container = al_load_bitmap("bossUI.png");
	if (!boss_UI_container)
		game_abort("failed to load image: bossUI.png");

	enemy_animation_default = al_load_bitmap("elemental.png");
	if (!enemy_animation_default)
		game_abort("failed to load animation sheet: elemental.png");
	enemy_animation_array[0].sheet = al_load_bitmap("elemental.png");
	if (!enemy_animation_array[0].sheet)
		game_abort("failed to load animation sheet: elemental.png");
	enemy_animation_array[0].cell_rows = 1;
	enemy_animation_array[0].cell_columns = 4;
	enemy_animation_array[0].cell_height = 64;
	enemy_animation_array[0].cell_width = 64;
	enemy_animation_array[0].last_col = 4;
	enemy_animation_array[0].current_row = 0;
	enemy_animation_array[0].current_column = 0;
	enemy_animation_array[0].frame_repeat = 0;
	enemy_animation_array[0].dupe_frames = 5;
	enemy_death_sfx = al_load_sample("elemental_death_sfx.ogg");
	if (!enemy_death_sfx)
		game_abort("failed to load enemy death sfx");

	
	img_laser_array = load_bitmap_resized("laser_array.png", \
		SCREEN_W / 16, SCREEN_H);
	img_laser_array_horizontal = load_bitmap_resized("laser_array_horizontal.png", \
		SCREEN_W, SCREEN_H / 16);
	laser_array_sfx = al_load_sample("Slide_Electronic_01.ogg");
	if (!laser_array_sfx)
		game_abort("failed to load laser array sfx");

	player_laser_sfx = al_load_sample("laser_sfx.ogg");
	if (!player_laser_sfx)
		game_abort("failed to load player laser sfx");
	upgrade_sfx = al_load_sample("upgrade_sfx.ogg");
	if (!upgrade_sfx)
		game_abort("failed to load upgrade sfx");

	start_bgm = al_load_sample("Brothers_in_Battle.ogg");
	if (!start_bgm)
		game_abort("failed to load audio: Brothers_in_Battle.ogg");

	boss_bgm_A = al_load_sample("Unhappy_Encounter.ogg");
	if (!boss_bgm_A)
		game_abort("failed to load audio: Unhappy_Encounter.ogg");
	boss_bgm_B = al_load_sample("Final_Phase.ogg");
	if (!boss_bgm_B)
		game_abort("failed to load audio: Final_Phase.ogg");

	warning_sfx = al_load_sample("Warning-sound.ogg");
	if (!warning_sfx)
		game_abort("failed to load audio: Warning-sound.ogg");

	ingame_timer = al_create_timer(1.0f/100000);
	if (!ingame_timer)
		game_abort("failed to create ingame timer");

	script = fopen("level_script.txt", "r");
	if (!script)
		game_abort("failed to load level script");

	dialogue = fopen("stage_intro.txt", "r");
	if (!dialogue)
		game_abort("failed to load dialogue script");

	img_bullet = al_load_bitmap("image12.png");
	if (!img_bullet)
		game_abort("failed to load image: image12.png");
	octa_charge_shot = load_bitmap_resized("octa_charge_shot.png", 34, 68);
	if (!octa_charge_shot)
		game_abort("failed to load image: octa_charge_shot.png");
	octa_charge_shot_sfx = al_load_sample("energy_blast_sfx.ogg");
	if (!octa_charge_shot_sfx)
		game_abort("failed to load octa charge shot sfx");

	enemy_bullet_1 = al_load_bitmap("enemybullet1.png");
	if (!enemy_bullet_1)
		game_abort("failed to load image: enemybullet1.png");

	small_explosion_sfx = al_load_sample("small_explosion_sfx.ogg");
	if (!small_explosion_sfx)
		game_abort("failed to load small explosion sfx");

	ult_sfx_A = al_load_sample("ult_sfx_A.ogg");
	ult_sfx_B = al_load_sample("ult_sfx_B.ogg");

	/* Results Scene resources*/
	results_img_background = al_load_bitmap("results-bg.png");
	if (!results_img_background)
		game_abort("failed to load image: results-bg.png");

	/* Game Over Scene resources*/
	game_over_img_background = al_load_bitmap("game-over-text.png");
	if (!game_over_img_background)
		game_abort("failed to load image: game-over-text.png");

	game_over_bgm = al_load_sample("favorite-colors.ogg");
	if (!game_over_bgm)
		game_abort("failed to load audio: favorite-colors.ogg");

	/* Titlecard resources*/
	img_titlecard = load_bitmap_resized("titlecard.png", SCREEN_W, SCREEN_H);

	// Change to first scene.
	//game_change_scene(SCENE_MENU);
	transition_target = SCENE_TITLECARD;
	game_change_scene(SCENE_TRANSITION);
}

void game_start_event_loop(void) {
	bool done = false;
	ALLEGRO_EVENT event;
	int redraws = 0;
	while (!done) {
		al_wait_for_event(game_event_queue, &event);
		if (active_scene == SCENE_EXIT) {
			// Exit Sequence
			game_log("Exiting game.");
			done = true;
		} else if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
			// Event for clicking the window close button.
			game_log("Window close button clicked");
			done = true;
		} else if (event.type == ALLEGRO_EVENT_TIMER) {
			// Event for redrawing the display.
			if (event.timer.source == game_update_timer)
				// The redraw timer has ticked.
				redraws++;
		} else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
			// Event for keyboard key down.
			game_log("Key with keycode %d down", event.keyboard.keycode);
			key_state[event.keyboard.keycode] = true;
			on_key_down(event.keyboard.keycode);
		} else if (event.type == ALLEGRO_EVENT_KEY_UP) {
			// Event for keyboard key up.
			game_log("Key with keycode %d up", event.keyboard.keycode);
			key_state[event.keyboard.keycode] = false;
			on_key_up(event.keyboard.keycode);
		} else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
			// Event for mouse key down.
			game_log("Mouse button %d down at (%d, %d)", event.mouse.button, event.mouse.x, event.mouse.y);
			mouse_state[event.mouse.button] = true;
			on_mouse_down(event.mouse.button, event.mouse.x, event.mouse.y);
		} else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
			// Event for mouse key up.
			game_log("Mouse button %d up at (%d, %d)", event.mouse.button, event.mouse.x, event.mouse.y);
			mouse_state[event.mouse.button] = false;
		} else if (event.type == ALLEGRO_EVENT_MOUSE_AXES) {
			if (event.mouse.dx != 0 || event.mouse.dy != 0) {
				// Event for mouse move.
				//game_log("Mouse move to (%d, %d)", event.mouse.x, event.mouse.y); // (Excessive logging causes lag.)
				mouse_x = event.mouse.x;
				mouse_y = event.mouse.y;
			} else if (event.mouse.dz != 0) {
				// Event for mouse scroll.
				game_log("Mouse scroll at (%d, %d) with delta %d", event.mouse.x, event.mouse.y, event.mouse.dz);
			}
		}
		// TODO: Process more events and call callbacks by adding more
		// entries inside Scene.

		// Redraw
		if (redraws > 0 && al_is_event_queue_empty(game_event_queue)) {
			// if (redraws > 1)
			// 	game_log("%d frame(s) dropped", redraws - 1);
			// Update and draw the next frame.
			game_update();
			game_draw();
			redraws = 0;
		}
	}
}

void game_update(void) {
	if (active_scene == SCENE_MENU) {
		// Advance main bg's frames.
		if (main_img_background_anim.frame_repeat < \
			main_img_background_anim.dupe_frames) {

			main_img_background_anim.frame_repeat++;
		}
		else if (main_img_background_anim.current_column\
			== main_img_background_anim.last_col - 1 && \
			main_img_background_anim.current_row == \
			main_img_background_anim.cell_rows - 1) {

			main_img_background_anim.current_column = 0;
			main_img_background_anim.current_row = 0;
			main_img_background_anim.frame_repeat = 0;
		}
		else if (main_img_background_anim.current_column\
			== main_img_background_anim.cell_columns - 1) {
			main_img_background_anim.current_column = 0;
			main_img_background_anim.current_row++;
			main_img_background_anim.frame_repeat = 0;
		}
		else {
			main_img_background_anim.current_column++;
			main_img_background_anim.frame_repeat = 0;
		}
	}
	else if (active_scene == SCENE_START) {
		if (!dying && lives == 0) {
			transition_target = SCENE_GAME_OVER;
			game_change_scene(SCENE_TRANSITION);
			return;
		}
		
		// TODO: If there are active dialogue flags, do not return. (Paused timer, but continue updating)
		// Open and process dialogue events.
		// Continue to the next event once Enter is hit.
		
		if (game_paused) {
			return;
		}

		// TODO: Keep track of active enemy and bullet count for dialogue loading. (Would be awful if you get interrupted while
		// dodging remaining bullets!)
		

		if (in_focus) {
			player_move_speed = 0.5;
		}
		else {
			player_move_speed = 2;
		}

		// Background Scrolling for SCENE_START.

		if(start_scene_phase == 0) bg_pos_y += scroll_speed; 
		else if (start_scene_phase == 1) {
			bg_pos_y += 0.75 * scroll_speed;
			layer_1_pos_y += scroll_speed;
		}
		else if (start_scene_phase == 2) {
			bg_pos_y += 0.5 * scroll_speed;
			layer_1_pos_y += 0.75 * scroll_speed;
			layer_2_pos_y += scroll_speed;
		}
		else if (start_scene_phase >= 3) {
			bg_pos_y += 0.25 * scroll_speed;
			layer_1_pos_y += 0.5 * scroll_speed;
			layer_2_pos_y += 0.75 * scroll_speed;
			layer_3_pos_y += scroll_speed;
		}

		player.vx = player.vy = 0;
		if (!respawning && !dying && !ult_active) { // Move player if not dying or respawning or using ult.
			if (key_state[ALLEGRO_KEY_UP] || key_state[ALLEGRO_KEY_W])
				player.vy -= player_move_speed;
			if (key_state[ALLEGRO_KEY_DOWN] || key_state[ALLEGRO_KEY_S])
				player.vy += player_move_speed;
			if (key_state[ALLEGRO_KEY_LEFT] || key_state[ALLEGRO_KEY_A])
				player.vx -= player_move_speed;
			if (key_state[ALLEGRO_KEY_RIGHT] || key_state[ALLEGRO_KEY_D])
				player.vx += player_move_speed;
			// 0.71 is (1/sqrt(2)).
			player.y += player.vy * 4 * (player.vx ? 0.71f : 1);
			player.x += player.vx * 4 * (player.vy ? 0.71f : 1);
		}
		
		
		// Limit the player's collision box inside the frame.
		// (x, y axes can be separated.)
		if (player.x < 0)
			player.x = 0;
		else if (player.x > SCREEN_W)
			player.x = SCREEN_W;
		if (player.y < 0)
			player.y = 0;
		else if (player.y > SCREEN_H - 24)
			player.y = SCREEN_H - 24;

		// Advance player animation's frames.
		if (player.animations[0].frame_repeat < \
			player.animations[0].dupe_frames) {

			player.animations[0].frame_repeat++;
		}
		else if (player.animations[0].current_column\
			== player.animations[0].last_col - 1 && \
			player.animations[0].current_row == \
			player.animations[0].cell_rows - 1) {

			player.animations[0].current_column = 0;
			player.animations[0].current_row = 0;
			player.animations[0].frame_repeat = 0;
		}
		else if (player.animations[0].current_column\
			== player.animations[0].cell_columns - 1) {
			player.animations[0].current_column = 0;
			player.animations[0].current_row++;
			player.animations[0].frame_repeat = 0;
		}
		else {
			player.animations[0].current_column++;
			player.animations[0].frame_repeat = 0;
		}

		double now = al_get_timer_count(ingame_timer);

		if (respawning) last_damage_timestamp = now;
		if ((now - last_damage_timestamp) / 100000 <= MAX_DAMAGE_COOLDOWN || ult_active) player_invulnerable = true;
		else player_invulnerable = false;
		
		// Handling Ult.
		if (ult_active) {
			if (sync_percentage != 1.0f) {
				ult_hitbox.hitbox_radius = 200 * ((now - ult_trigger_timestamp) / 100000) / 2.0f;

				if ((now - ult_trigger_timestamp) / 100000 >= 2.0f) {
					sync_percentage -= 0.5f;
					ult_active = false;
				}
			}
			else {
				ult_hitbox.hitbox_radius = 400 * ((now - ult_trigger_timestamp) / 100000) / 3.0f;
				if ((now - ult_trigger_timestamp) / 100000 >= 3.0f) {
					sync_percentage = 0;
					ult_active = false;
				}
			}
		}

		// Handle Octa's state.
		if (mouse_state[2] && (now - last_octa_switch_timestamp)/100000 >= OCTA_SWITCH_COOLDOWN) {
			is_octa_dormant = !is_octa_dormant;
			last_octa_switch_timestamp = now;
		}
		if (!is_octa_dormant) {
			octa.x = mouse_x - octa.hitbox_source_x_offset;
			octa.y = mouse_y - octa.hitbox_source_y_offset;
		}
		else {
			octa.x = player.x;
			octa.y = player.y;
		}

		// Update bullet coordinates.
		int i;
		for (i = 0;i < MAX_BULLET;i++) {
			if (bullets[i].hidden == true)
				continue;
			bullets[i].x += bullets[i].vx;
			bullets[i].y += bullets[i].vy;

			// Apply proper collision according to hostility.
			// TODO: Consider heuristics when there are many visible objects to check.
			// TODO: Make enemy compatible with animations for death animation.
			// TODO: %chance for enemy to drop power-ups.
			
			if (bullets[i].hostile) { // Hostile bullet vs player.
				if (!dying && !respawning)
				if ((circle_collision_test(bullets[i], player)\
					||circle_collision_test(bullets[i], octa))\
					&& (now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN) {
					al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
					game_log("Player takes damage.");
					last_damage_timestamp = now;
					player.current_HP -= bullets[i].base_damage;
					bullets[i].hidden = true;
					combo = 1; // Reset combo.
					if (player.current_HP <= 0) {
						death_routine();
					}
				}
				
				if(is_synchronizing)
					if (circle_line_collision_test(bullets[i],\
						player.x + player.hitbox_source_x_offset, \
						player.y + player.hitbox_source_y_offset, \
						octa.x + octa.hitbox_source_x_offset, \
						octa.y + octa.hitbox_source_y_offset)) {
						sync_rate = 0;
						last_unsync_timestamp = now;
						is_synchronizing = false;
					}

				if (ult_active) {
					if (circle_collision_test(bullets[i], ult_hitbox)) {
						al_play_sample(small_explosion_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &small_explosion_sfx_id);
						bullets[i].hidden = true;
					}
				}
			}
			else { // Friendly bullet vs enemy.
				int j;
				for (j = 0; j < MAX_ENEMY; j++) {
					if (enemies[j].hidden)
						continue;
					if (circle_collision_test(bullets[i], enemies[j])){
						al_play_sample(small_explosion_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &small_explosion_sfx_id);
						enemies[j].current_HP -= bullets[i].base_damage;
						if (enemies[j].current_HP < 0) {
							al_play_sample(enemy_death_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &enemy_death_sfx_id);
							score += 900 + 100 * combo;
							combo++;
							sync_percentage += 0.01;
							if (player_attack_level < 1) {
								if (player_attack_level + 0.1 > 1) {
									al_play_sample(upgrade_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &upgrade_sfx_id);
								}
							}
							else if (player_attack_level < 2.5) {
								if (player_attack_level + 0.1 > 2.5) {
									al_play_sample(upgrade_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &upgrade_sfx_id);
								}
							}
							else if (player_attack_level < 4) {
								if (player_attack_level + 0.1 > 4) {
									al_play_sample(upgrade_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &upgrade_sfx_id);
								}
							}
							player_attack_level += 0.1;
							if (sync_percentage > 1) sync_percentage = 1;
							enemies[j].hidden = true;
							active_enemy_count--;
						}
						bullets[i].hidden = true;
					}
				}

				// VS boss.
				if (start_scene_phase > 0) {
					if (boss.current_HP > 0) {
						if (circle_collision_test(bullets[i], boss)) {
							al_play_sample(small_explosion_sfx, 0.25, 0.2, 1.2, ALLEGRO_PLAYMODE_ONCE, &small_explosion_sfx_id);
							bullets[i].hidden = true;
							score += 10 * combo;
							combo++;
							sync_percentage += 0.0001;
							boss.current_HP -= bullets[i].base_damage;
						}
					}
				}
			}
			if (!pnt_in_rect(bullets[i].x, bullets[i].y, 0, 0, SCREEN_W, SCREEN_H)) {
				bullets[i].hidden = true;
			}
		}

		int j = 0;

		// Shoot if key is down and cool-down is over and not respawning.

		// Friendly bullets.
		if (!respawning && key_state[ALLEGRO_KEY_SPACE] && (now - last_shoot_timestamp)/100000 >= MAX_COOLDOWN) {
			last_shoot_timestamp = now;

			al_play_sample(player_laser_sfx, 0.25, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_laser_sfx_id);
			if (player_attack_level < 1) {
				for (i = 0; i < MAX_BULLET;i++) {
					if (bullets[i].hidden)
						break;
				}
				bullets[i].vx = 0;
				bullets[i].vy = player_bullet_speed;
				bullets[i].img = img_bullet;
				bullets[i].hidden = false;
				bullets[i].hostile = false;
				bullets[i].hitbox_radius = bullets[i].w / 2;
				bullets[i].base_damage = player_damage;
				bullets[i].x = player.x + 24;
				bullets[i].y = player.y - 66;

				while (i < MAX_BULLET) {
					if (bullets[i].hidden)
						break;
					i++;
				}

				bullets[i].vx = 0;
				bullets[i].vy = player_bullet_speed;
				bullets[i].img = img_bullet;
				bullets[i].hidden = false;
				bullets[i].hostile = false;
				bullets[i].hitbox_radius = bullets[i].w / 2;
				bullets[i].base_damage = player_damage;
				bullets[i].x = player.x - 24;
				bullets[i].y = player.y - 66;
			}
			else {
				int count = 0;
				for (i = 0; count < 2;i++) {
					if (bullets[i].hidden) {
						bullets[i].vx = 0;
						bullets[i].vy = player_bullet_speed;
						bullets[i].img = img_bullet;
						bullets[i].hidden = false;
						bullets[i].hostile = false;
						bullets[i].hitbox_radius = bullets[i].w / 2;
						bullets[i].base_damage = player_damage;
						bullets[i].x = player.x + 24 - \
							bullets[i].hitbox_radius + \
							2 * bullets[i].hitbox_radius * count;
						bullets[i].y = player.y - 66;
						count++;
					}

				}

				count = 0;
				while (count < 2) {
					if (bullets[i].hidden) {
						bullets[i].vx = 0;
						bullets[i].vy = player_bullet_speed;
						bullets[i].img = img_bullet;
						bullets[i].hidden = false;
						bullets[i].hostile = false;
						bullets[i].hitbox_radius = bullets[i].w / 2;
						bullets[i].base_damage = player_damage;
						bullets[i].x = player.x - 24 - \
							bullets[i].hitbox_radius + \
							2 * bullets[i].hitbox_radius * count;
						bullets[i].y = player.y - 66;
						count++;
					}
					i++;
				}

				if (player_attack_level > 2.5) {
					count = 0;
					while (count < 2) {
						if (bullets[i].hidden) {
							bullets[i].vx = 0;
							bullets[i].vy = player_bullet_speed;
							rotate_vector(&bullets[i].vx, &bullets[i].vy, (double)6 - (double)12 * (double)count);
							bullets[i].img = img_bullet;
							bullets[i].hidden = false;
							bullets[i].hostile = false;
							bullets[i].hitbox_radius = bullets[i].w / 2;
							bullets[i].base_damage = player_damage;
							bullets[i].x = player.x - 24 - \
								3 * bullets[i].hitbox_radius + \
								count * (6 * bullets[i].hitbox_radius + 48);
							bullets[i].y = player.y - 66 + bullets[i].hitbox_radius;
							count++;
						}
						i++;
					}

					count = 0;
					while (count < 2) {
						if (bullets[i].hidden) {
							bullets[i].vx = 0;
							bullets[i].vy = player_bullet_speed;
							rotate_vector(&bullets[i].vx, &bullets[i].vy, (double)-6 + (double)12 * (double)count);
							bullets[i].img = img_bullet;
							bullets[i].hidden = false;
							bullets[i].hostile = false;
							bullets[i].hitbox_radius = bullets[i].w / 2;
							bullets[i].base_damage = player_damage;
							bullets[i].x = player.x - 24 + \
								3 * bullets[i].hitbox_radius + \
								count * (-6 * bullets[i].hitbox_radius + 48);
							bullets[i].y = player.y - 66 + bullets[i].hitbox_radius;
							count++;
						}
						i++;
					}

					if (player_attack_level > 4) {
						count = 0;
						while (count < 2) {
							if (bullets[i].hidden) {
								bullets[i].vx = 0;
								bullets[i].vy = player_bullet_speed;
								bullets[i].img = img_bullet;
								bullets[i].hidden = false;
								bullets[i].hostile = false;
								bullets[i].hitbox_radius = bullets[i].w / 2;
								bullets[i].base_damage = player_damage;
								bullets[i].x = player.x - 24 + \
									count * 48;
								bullets[i].y = player.y - 66 - 2 * bullets[i].hitbox_radius;
								count++;
							}
							i++;
						}
					}
				}

			}
		}
		

		// Charge Shots from Octa.
		if (!respawning && mouse_state[1] && (now - last_release_charge_timestamp) / 100000 >= MIN_CHARGE_TIME) {
			double charge_time = (now - last_release_charge_timestamp) / 100000;
			if (charge_time > 16) {
				charge_time = 16;
			}
			al_play_sample(octa_charge_shot_sfx, 1, 0, 1.5 - charge_time / 16, ALLEGRO_PLAYMODE_ONCE, &octa_charge_shot_sfx_id);
			last_release_charge_timestamp = now;

			for (i = 0; i < MAX_BULLET;i++) {
				if (bullets[i].hidden)
					break;
			}

			bullets[i].vx = 0;
			bullets[i].vy = player_bullet_speed*sqrt(charge_time);
			bullets[i].img = octa_charge_shot;
			bullets[i].w = al_get_bitmap_width(octa_charge_shot);
			bullets[i].h = al_get_bitmap_height(octa_charge_shot);
			bullets[i].hidden = false;
			bullets[i].hostile = false;
			bullets[i].hitbox_radius = bullets[i].w/2;
			bullets[i].hitbox_source_y_offset = -5;
			
			int level = 1;
			if (player_attack_level > 1) {
				if (player_attack_level < 2.5) level = 2;
				else if (player_attack_level < 4) level = 3;
				else level = 4;
			}
			
			bullets[i].base_damage = level * player_damage*sqrt(charge_time);
			bullets[i].x = octa.x + octa.hitbox_source_x_offset;
			bullets[i].y = octa.y + octa.hitbox_source_y_offset;

		}

		// Scheduled script of enemy spawns, color alterations, formations, and firing patterns.

		if(now/100000 > next_event_timestamp && !script_done){
			int instance_count;
			if (fscanf(script, "%d", &instance_count) != 1) {
				game_abort("Failed to parse instance count at timestamp = %lf.", next_event_timestamp);
			}
			active_enemy_count += instance_count;
			
			for (i = 0; i < instance_count; i++) {
				// Look for a hidden enemy to be used.
				int j;
				for (j = 0; j < MAX_ENEMY; j++) {
					if (enemies[j].hidden) {
						game_log("Found usable enemy at array position %d", j);
						break;
					}
				}


				if(fscanf(script, "%d %f %c %c", &enemies[j].id, &enemies[j].max_HP,\
				&enemies[j].attack_mode, &enemies[j].movement_mode) != 4)
					game_abort("Failed to parse instance parameters at timestamp = %lf.", next_event_timestamp);
				
				enemies[j].animations[0].sheet = \
					enemy_animation_array[enemies[j].id].sheet;
				enemies[j].animations[0].cell_rows = \
					enemy_animation_array[enemies[j].id].cell_rows;
				enemies[j].animations[0].cell_columns = \
					enemy_animation_array[enemies[j].id].cell_columns;
				enemies[j].animations[0].cell_height = \
					enemy_animation_array[enemies[j].id].cell_height;
				enemies[j].animations[0].cell_width = \
					enemy_animation_array[enemies[j].id].cell_width;
				enemies[j].animations[0].last_col = \
					enemy_animation_array[enemies[j].id].last_col;
				enemies[j].animations[0].current_row = \
					enemy_animation_array[enemies[j].id].current_row;
				enemies[j].animations[0].current_column = \
					enemy_animation_array[enemies[j].id].current_column;
				enemies[j].animations[0].frame_repeat = \
					enemy_animation_array[enemies[j].id].frame_repeat;
				enemies[j].animations[0].dupe_frames = \
					enemy_animation_array[enemies[j].id].dupe_frames;
				enemies[j].w = enemies[j].animations[0].cell_width;
				enemies[j].h = enemies[j].animations[0].cell_height;

				// Set parameters according to the script.
				if (enemies[j].movement_mode == 'C') {
					int frames;
					if (fscanf(script, "%f %f %f %f %d"\
						, &enemies[j].x, &enemies[j].y,\
						&enemies[j].ax, &enemies[j].ay,\
						&frames) != 5) {
						game_abort("Failed to parse instance parameters at timestamp = %lf.", next_event_timestamp);
					}
					enemies[j].vx = enemies[j].ax / frames;
					enemies[j].vy = enemies[j].ay / frames;
				}
				else {
					if (fscanf(script, "%f %f %f %f %f %f"\
						, &enemies[j].x, &enemies[j].y, &enemies[j].vx\
						, &enemies[j].vy, &enemies[j].ax, &enemies[j].ay) != 6) {
						game_abort("Failed to parse instance parameters at timestamp = %lf.", next_event_timestamp);
					}
				}



				// Fill current HP to max.
				enemies[j].current_HP = enemies[j].max_HP;

				// Fixed collision damage.
				enemies[j].base_damage = player.max_HP / 4;

				// Set firing cooldown according to attack mode.

				if (enemies[j].attack_mode == 'A') {
					enemies[j].firing_cooldown = 1.0f;
				}
				else { // Default
					enemies[j].firing_cooldown = 2.0f;
				}
				
				enemies[j].hidden = false;
				enemies[j].spawn_timestamp = now;
				enemies[j].hostile = true;

			}

			if (fscanf(script, "%lf", &next_event_timestamp) == EOF) {
				script_done = true; // Mark script as done.
				game_log("EOF reached.");
			}
			else {
				game_log("Next Event Timestamp: %lf", next_event_timestamp);
			}
		}

		// Update enemy coordinates according to their variables and modes.
		for (i = 0; i < MAX_ENEMY; i++) {
			if (enemies[i].hidden)
				continue;

			// Ult handling.
			if (ult_active) {
				if (circle_collision_test(enemies[i], ult_hitbox)) {
					score += 100;
					al_play_sample(enemy_death_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &enemy_death_sfx_id);
					active_enemy_count--;
					enemies[i].hidden = true;
				}
			}

			// Advance enemy animation's frames.
			if (enemies[i].animations[0].frame_repeat < \
				enemies[i].animations[0].dupe_frames) {

				enemies[i].animations[0].frame_repeat++;
			}
			else if (enemies[i].animations[0].current_column\
				== enemies[i].animations[0].last_col - 1 &&\
				enemies[i].animations[0].current_row ==\
				enemies[i].animations[0].cell_rows - 1) {

				enemies[i].animations[0].current_column = 0;
				enemies[i].animations[0].current_row = 0;
				enemies[i].animations[0].frame_repeat = 0;
			}
			else if (enemies[i].animations[0].current_column\
				== enemies[i].animations[0].cell_columns - 1) {
				enemies[i].animations[0].current_column = 0;
				enemies[i].animations[0].current_row++;
				enemies[i].animations[0].frame_repeat = 0;
			}
			else {
				enemies[i].animations[0].current_column++;
				enemies[i].animations[0].frame_repeat = 0;
			}

			if (enemies[i].movement_mode == 'A') {
				// Movement Mode A: Straight line according to initial velocity.
				// No changes.
				enemies[i].x += enemies[i].vx;
				enemies[i].y += enemies[i].vy;
				enemies[i].vx += enemies[i].ax;
				enemies[i].vy += enemies[i].ay;
			}
			else if (enemies[i].movement_mode == 'B') {
				// Movement Mode B: Homing. (Arcing towards player)

				if ((player.x - enemies[i].x) * (enemies[i].ax) < 0)
					enemies[i].ax *= -1;
 
				if ((player.y - enemies[i].y) * (enemies[i].ay) < 0)
			 		enemies[i].ay *= -1;

			 	enemies[i].x += enemies[i].vx;
				enemies[i].y += enemies[i].vy;
				enemies[i].vx += enemies[i].ax;
				enemies[i].vy += enemies[i].ay;
			}
			else if (enemies[i].movement_mode == 'C' &&\
				(enemies[i].vx != 0 || enemies[i].vy != 0)) {
				enemies[i].x += enemies[i].vx;
				enemies[i].y += enemies[i].vy;
				enemies[i].ax -= enemies[i].vx;
				enemies[i].ay -= enemies[i].vy;
				if (fabs(enemies[i].ax) < 0.01 && fabs(enemies[i].ay) < 0.01) {
					enemies[i].vx = 0;
					enemies[i].vy = 0;
				}
			}
			else if (enemies[i].movement_mode == 'D') {
				double cosine = cos((now - enemies[i].spawn_timestamp)/100000);
				enemies[i].vx += enemies[i].ax;
				enemies[i].vy += enemies[i].ay;
				enemies[i].x += enemies[i].vx * cosine;
				enemies[i].y += enemies[i].vy * cosine;
			}
			else if (enemies[i].movement_mode == 'E') {
				enemies[i].x += enemies[i].vx;
				enemies[i].y += enemies[i].vy;
				// Prevent wonky ricochets.
				if((now - enemies[i].spawn_timestamp) / 100000 > 1.5f){ 
					if (enemies[i].y <= 0 || enemies[i].y >= SCREEN_H) {
						enemies[i].vy *= -1;
						enemies[i].y += enemies[i].vy;
					}
					if (enemies[i].x <= 0 || enemies[i].x >= SCREEN_W) {
						enemies[i].vx *= -1;
						enemies[i].x += enemies[i].vx;
					}
				}
				enemies[i].vx += enemies[i].ax;
				enemies[i].vy += enemies[i].ay;
			}

			if ((now - enemies[i].spawn_timestamp)/100000 > 10.0f) {
				if (enemies[i].movement_mode == 'C') {
					enemies[i].vx = 0;
					enemies[i].vy = 1;
					enemies[i].ax = 0;
					enemies[i].ay = 0.25;
				}
				enemies[i].movement_mode = 'A';
			}


			// TODO: Attack Modes.
			if ((now - enemies[i].last_shoot_timestamp) / 100000 >= enemies[i].firing_cooldown) {
				if (enemies[i].attack_mode == 'A') {
					// Attack Mode A: Single Aimed Rapid Fire

					enemies[i].last_shoot_timestamp = now;
					for (j = 0; j < MAX_BULLET; j++) {
						if (bullets[j].hidden) {
							break;
						}
					}
					unit_vector(player.x - enemies[i].x, player.y - enemies[i].y, \
						&bullets[j].vx, &bullets[j].vy);
					bullets[j].vx *= 4;
					bullets[j].vy *= 4;
					bullets[j].x = enemies[i].x;
					bullets[j].y = enemies[i].y;
					bullets[j].hidden = false;
					bullets[j].hostile = true;
					bullets[j].hitbox_radius = bullets[j].w/2;
					bullets[j].base_damage = 10;
					bullets[j].img = enemy_bullet_1;

				}
				else if (enemies[i].attack_mode == 'B' || enemies[i].attack_mode == 'C') {
					// Attack Mode B: 3x Aimed Scatter Shot
					// Attack Mode C: 5x Aimed Scatter Shot
					int count;
					if (enemies[i].attack_mode == 'B') count = 3;
					else count = 5;

					enemies[i].last_shoot_timestamp = now;
					int k = 0;
					int arr[5];
					for (j = 0; j < MAX_BULLET; j++) {
						if (bullets[j].hidden) {
							arr[k] = j;
							bullets[arr[k]].img = enemy_bullet_1;
							bullets[arr[k]].x = enemies[i].x;
							bullets[arr[k]].y = enemies[i].y;
							bullets[arr[k]].hidden = false;
							bullets[arr[k]].hostile = true;
							bullets[arr[k]].hitbox_radius = bullets[arr[k]].w/2;
							bullets[arr[k]].base_damage = 10;
							k++;
							if (k == count) break;
						}
					}

					// Set the first vector and get the remaining ones iteratively.
					unit_vector(player.x - enemies[i].x, player.y - enemies[i].y, \
						&bullets[arr[0]].vx, &bullets[arr[0]].vy);
					if (enemies[i].attack_mode == 'B') rotate_vector(&bullets[arr[0]].vx, &bullets[arr[0]].vy, -15);
					else rotate_vector(&bullets[arr[0]].vx, &bullets[arr[0]].vy, -25);
					bullets[arr[0]].vx *= 1.5;
					bullets[arr[0]].vy *= 1.5;
					
					for (k = 1; k < count; k++) {
						bullets[arr[k]].vx = bullets[arr[k - 1]].vx;
						bullets[arr[k]].vy = bullets[arr[k - 1]].vy;
						rotate_vector(&bullets[arr[k]].vx, &bullets[arr[k]].vy, (double) 10 * k);
					}
				}
				else if (enemies[i].attack_mode == 'D') {
					// Attack Mode D: 5x Circle Shot
					enemies[i].last_shoot_timestamp = now;
					double t = now / 100000;
					float vx = 1.0;
					float vy = 0;
					rotate_vector(&vx, &vy, 360 * sin(t));
					int count = 0;
					int j = 0;
					for (j = 0; count < 5; j++) {
						if (bullets[j].hidden) {
							bullets[j].hidden = false;
							bullets[j].hostile = true;
							bullets[j].img = enemy_bullet_1;
							rotate_vector(&vx, &vy, (double) count * (double) 72);
							bullets[j].vx = vx;
							bullets[j].vy = vy;
							bullets[j].x = enemies[i].x;
							bullets[j].y = enemies[i].y;
							bullets[j].hitbox_radius = bullets[j].w / 2;
							bullets[j].base_damage = 10;
							count++;
						}
					}
				}
			}

			

			if(!dying && !respawning)
			if (circle_collision_test(enemies[i], player) && (now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN) {
				al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= enemies[i].base_damage;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}

			if (is_synchronizing)
				if (circle_line_collision_test(enemies[i],\
					player.x+ player.hitbox_source_x_offset,\
					player.y + player.hitbox_source_y_offset,\
					octa.x + octa.hitbox_source_x_offset,\
					octa.y + octa.hitbox_source_y_offset)) {
					sync_rate = 0;
					last_unsync_timestamp = now;
					is_synchronizing = false;
				}

			if (!pnt_in_rect(enemies[i].x, enemies[i].y, 0, 0, SCREEN_W, SCREEN_H)){
				enemies[i].hidden = true;
				active_enemy_count--;
				combo = 1; // Resets combo on miss.
			}
		}

		// Handling Octa's state.
		

		// Start Synchronizing 
		if ((now - last_unsync_timestamp) / 100000 >= MIN_SYNC_TIME) {
			is_synchronizing = true;
			float dx = player.x + player.hitbox_source_x_offset - octa.x - octa.hitbox_source_x_offset;
			float dy = player.y + player.hitbox_source_y_offset - octa.y - octa.hitbox_source_y_offset;
			float length_coeff = sqrt((double)dx * dx + (double)dy * dy)/200;
			if (length_coeff > 2) length_coeff = 2;
			
			if (sync_rate < 0.00025) {
				sync_rate += 0.0000001;
			}
			else sync_rate = 0.00025;

			sync_percentage += sync_rate * length_coeff;
			if (sync_percentage > 1) {
				sync_percentage = 1;
			}

		}


		// Get ready for transition into boss battle.
		if (start_scene_phase == 0) {
			if (script_done) {
				if (active_enemy_count == 0 && transition[0] == false && transition[1] == false) {
					scroll_speed *= 1.5;
					transition[0] = true;
					transition_timestamp = now;
					al_stop_sample(&start_bgm_id);
					if (!al_play_sample(warning_sfx, 1, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, &warning_sfx_id))
						game_abort("failed to play warning sfx");
					
				}
			}

			if (transition[0]) {
				if ((now - transition_timestamp) / 100000 >= 5.0f) {
					al_stop_sample(&warning_sfx_id);
					al_play_sample(boss_bgm_A, 1, 0, 1, ALLEGRO_PLAYMODE_LOOP, &boss_bgm_A_id);
					transition[0] = false;
					transition[1] = true;

				}
			}

			if (transition[1]) {
				if ((now - transition_timestamp) / 100000 >= 6.0f) {
					transition[1] = false;
					transition[0] = true;
					start_scene_phase = 1;
					layer_1_pos_y = -SCREEN_H;
					// TODO: Initialize boss and set him up for entry.
					boss.hidden = false;
					boss.img = img_boss;
					boss.alpha = 255;
					boss.w = al_get_bitmap_width(img_boss);
					boss.h = al_get_bitmap_height(img_boss);
					boss.x = SCREEN_W / 2;
					boss.y = -(boss.h / 2 + 25);
					boss.vx = 0;
					boss.vy = 2;
					boss.base_damage = 10;
					boss.current_HP = 24000;
					boss.max_HP = 24000;
					boss.hitbox_radius = boss.h / 2 + 15;
					boss.hitbox_source_x_offset = 0;
					boss.hitbox_source_y_offset = 0;
				}
				if(start_bg_alpha < 100) start_bg_alpha++;
			}
		}
		if (start_scene_phase == 1) {
			if ((circle_collision_test(player, boss) || circle_collision_test(octa, boss)) \
				&& (now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN) {
				al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= 25;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}

			// Ult handling.
			if (ult_active && !boss_damaged_by_ult) {
				if (circle_collision_test(boss, ult_hitbox)) {
					boss.current_HP -= 3000*sync_percentage;
					boss_damaged_by_ult = true;
				}
			}

			if (transition[0]) {
				boss.x += boss.vx;
				boss.y += boss.vy;
				// Entry Completed, enter phase 1-1 of boss.
				if (boss.y >= 25 + boss.h/2) {
					game_log("Entering Phase 1-1.");
					boss.vx = 0;
					boss.vy = 0;
					transition[0] = false;
					transition[1] = true;
					transition_timestamp = now;
					boss.last_shoot_timestamp = now;
				}
			}
			if (transition[1]) {
				// Phase 1-1 pattern: Double Bullet Storm!
				if ((now - boss.last_shoot_timestamp)/100000 >= 0.75f) {
					int count = 0;

					if(((int)(now/100000))%2){
						float vx = -2;
						float vy = 0;
						rotate_vector(&vx, &vy, ((double)(rand() % 15) + 15));
						
						for (i = 0; count < 20;i++) {
							if (bullets[i].hidden) {
								bullets[i].hidden = false;
								bullets[i].hostile = true;
								bullets[i].img = enemy_bullet_1;
								bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img)/2;
								bullets[i].x = boss.x - 30;
								bullets[i].y = boss.y + 20;
								bullets[i].vx = -vx;
								bullets[i].vy = -vy;
								bullets[i].base_damage = boss.base_damage;
								rotate_vector(&vx, &vy, 6);
								count++;
							}
						}
					}
					else {
						float vx = 2;
						float vy = 0;
						rotate_vector(&vx, &vy, -((double)(rand() % 15) + 15));

						for (i = 0; count < 20;i++) {
							if (bullets[i].hidden) {
								bullets[i].hidden = false;
								bullets[i].hostile = true;
								bullets[i].img = enemy_bullet_1;
								bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
								bullets[i].x = boss.x + 30;
								bullets[i].y = boss.y + 20;
								bullets[i].vx = -vx;
								bullets[i].vy = -vy;
								bullets[i].base_damage = boss.base_damage;
								rotate_vector(&vx, &vy, -6);
								count++;
							}
						}
					}

					boss.last_shoot_timestamp = now;
				}
				// Enter phase 1-2.
				if (boss.current_HP <= 21000) {
					game_log("Entering Phase 1-2.");
					double time_taken = (now - transition_timestamp)/100000;
					if (time_taken < 60) score += 10000*(1 - time_taken/60);
					transition[1] = false;
					transition[2] = true;
					transition_timestamp = now;
					boss.vx = 0;
					boss.vy = 0;
					boss.base_damage = 15;
					thunder_cap = 4;
				}
			}
			if (transition[2]) {
				int count = 0;

				// Horizontal SHM
				boss.vx = 1.5*cos((now - transition_timestamp) / 100000);
				boss.x += boss.vx;
				// Phase 1-2 pattern A: Lv. 1 Thunder

				if ((now - boss.last_shoot_timestamp) / 100000 > 1.0f) {
					if (point_stack_counter < thunder_cap && boss.current_HP > 16500) { // Charge...
						point_stack[++point_stack_counter].x = player.x + 80 - rand() % 160;
						point_stack[point_stack_counter].y = player.y + 40 - rand() % 80;
					}
					else if (point_stack_counter > -1) { // RELEASE!
						release_lightning = true;
						al_play_sample(thunder_sfx, 1, 0, 2, ALLEGRO_PLAYMODE_ONCE, &thunder_sfx_id);
						float width = al_get_bitmap_width(img_lightning);
						for (i = 0; i <= point_stack_counter; i++) {
							float target_x = point_stack[i].x - width / 2;
							float target_y = point_stack[i].y - width / 2;
							if (pnt_in_rect(player.x, player.y, \
								target_x, target_y, width, \
								width) && \
								(now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
								al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
								game_log("Player takes damage.");
								last_damage_timestamp = now;
								player.current_HP -= 1.5 * boss.base_damage;
								combo = 1; // Reset combo.
								if (player.current_HP <= 0) {
									death_routine();
								}
							}

						}
					}
					game_log("Queued lightning at (%f, %f)", \
						point_stack[point_stack_counter].x, \
					point_stack[point_stack_counter].y);
					// Pattern B: Chasing Quints.
					float vx;
					float vy;
					float sx;
					float sy;
					if (((int)(now / 100000)) % 2) {
						sx = boss.x + 30;
						sy = boss.y + 20;
					}
					else {
						sx = boss.x - 30;
						sy = boss.y + 20;
					}
					unit_vector(player.x - sx, player.y - sy, &vx, &vy);
					for (i = 0; count < 5; i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
							bullets[i].hitbox_source_x_offset = 0;
							bullets[i].hitbox_source_y_offset = 0;
							bullets[i].x = sx + count * vx * bullets[i].hitbox_radius;
							bullets[i].y = sy + count * vy * bullets[i].hitbox_radius;
							bullets[i].vx = 7 * vx;
							bullets[i].vy = 7 * vy;
							bullets[i].base_damage = boss.base_damage;
							count++;
						}
					}

					boss.last_shoot_timestamp = now;
				}
				// Enter phase 1-3.
				// Enemy spawning.
				if (boss.current_HP <= 16500 && point_stack_counter == -1 \
					&& fabs((double)boss.x - (double)SCREEN_W/2) < 1) {
					game_log("Entering Phase 1-3.");

					double time_taken = (now - transition_timestamp) / 100000;
					if (time_taken < 60) score += 20000 * (1 - time_taken / 60);
					transition[2] = false;
					transition[3] = true;
					boss.x = SCREEN_W / 2;
					transition_timestamp = now;
					boss.base_damage = 12;
					count = 0;
					// Find 6 enemies.
					for (i = 0; count < 7; i++) {
						if (count == 3) count++;
						if (enemies[i].hidden) {
							active_enemy_count++;
							enemies[i].hidden = false;
							enemies[i].attack_mode = 'B';
							enemies[i].movement_mode = 'E';
							enemies[i].x = 100 + count * 100;
							enemies[i].y = 0;
							enemies[i].vx = 0;
							enemies[i].vy = 2;
							enemies[i].ax = 0;
							enemies[i].ay = 0;
							// Fill current HP to max.
							enemies[i].max_HP = 50;
							enemies[i].current_HP = enemies[i].max_HP;
							// Fixed collision damage.
							enemies[i].base_damage = player.max_HP / 4;
							enemies[i].firing_cooldown = 2.5f;
							enemies[i].spawn_timestamp = now;
							enemies[i].hostile = true;
							count++;
						}
					}
				}
			}
			if (transition[3]) {
				// Phase 1-3: Summon Elemental + Double Scatter
				if ((now - boss.last_shoot_timestamp) / 100000 >= 1.0f) {
					int count = 0;

					float vx = -2;
					float vy = 0;
					rotate_vector(&vx, &vy, ((double)(rand() % 15) + 15));

					for (i = 0; count < 15;i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
							bullets[i].x = boss.x - 30;
							bullets[i].y = boss.y + 20;
							bullets[i].vx = -vx;
							bullets[i].vy = -vy;
							bullets[i].base_damage = boss.base_damage;
							rotate_vector(&vx, &vy, 8);
							count++;
						}
					}
					vx = 2;
					vy = 0;
					rotate_vector(&vx, &vy, -((double)(rand() % 15) + 15));
					count = 0;
					for (i = 0; count < 15;i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
							bullets[i].x = boss.x + 30;
							bullets[i].y = boss.y + 20;
							bullets[i].vx = -vx;
							bullets[i].vy = -vy;
							bullets[i].base_damage = boss.base_damage;
							rotate_vector(&vx, &vy, -8);
							count++;
						}
					}
					

					boss.last_shoot_timestamp = now;
				}


				// Enter phase 2.
				if (boss.current_HP <= 12000) {
					game_log("Entering Phase 2-1.");
					double time_taken = (now - transition_timestamp) / 100000;
					if (time_taken < 60) score += 30000 * (1 - time_taken / 60);
					transition[3] = false;
					transition[0] = true;
					transition_timestamp = now;
					boss.base_damage = 20;
					start_scene_phase = 2;
					layer_2_pos_y = -SCREEN_H;
					laser_array.is_active = true;
					laser_array.horizontal = false;
					laser_array.vertical = true;
					laser_array.mirror = false;
					laser_array.y = 0;
					laser_array.v = 2.5;
					laser_array.a = 0.2;
					laser_array.v_limit = 4;
					laser_array.gap_halfwidth = 75;
					laser_array.vert_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
					al_play_sample(laser_array_sfx, 0.5, 0, 1, ALLEGRO_PLAYMODE_LOOP, &laser_array_sfx_id);
				}
			}
		}
		if (start_scene_phase == 2) {
			if (start_bg_alpha < 150) start_bg_alpha++;
			if ((circle_collision_test(player, boss) || circle_collision_test(octa, boss)) \
				&& (now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN) {
				al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= 25;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}
			// Ult handling.
			if (ult_active && !boss_damaged_by_ult) {
				if (circle_collision_test(boss, ult_hitbox)) {
					boss.current_HP -= 1500 * sync_percentage;
					boss_damaged_by_ult = true;
				}
			}

			if (transition[0]) {
				// Phase 2-1: Rapid Bullet Stream + Lv. 1 Laser Array
				laser_array_routine(now);

				if ((now - boss.last_shoot_timestamp) / 100000 >= 0.4f) {
					// Left Side
					for (i = 0; i < MAX_BULLET; i++) {
						if (bullets[i].hidden)
							break;
					}
					// Right Side
					for (j = i + 1; j < MAX_BULLET; j++) {
						if (bullets[j].hidden)
							break;
					}
					bullets[i].hidden = false;
					bullets[j].hidden = false;
					bullets[i].hostile = true;
					bullets[j].hostile = true;
					bullets[i].x = boss.x - 30;
					bullets[i].y = boss.y + 20;
					bullets[j].x = boss.x + 30;
					bullets[j].y = boss.y + 20;
					unit_vector(player.x - bullets[i].x, \
						player.y - bullets[i].y, &bullets[i].vx, &bullets[i].vy);
					unit_vector(player.x - bullets[j].x, \
						player.y - bullets[j].y, &bullets[j].vx, &bullets[j].vy);
					bullets[i].vx *= 2;
					bullets[i].vy *= 2;
					bullets[j].vx *= 2;
					bullets[j].vy *= 2;
					bullets[i].img = enemy_bullet_1;
					bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
					bullets[j].img = enemy_bullet_1;
					bullets[j].hitbox_radius = al_get_bitmap_width(bullets[j].img) / 2;

					boss.last_shoot_timestamp = now;
				}

				// Enter phase 2-2.
				if (boss.current_HP <= 9000) {
					game_log("Entering Phase 2-2.");
					double time_taken = (now - transition_timestamp) / 100000;
					if (time_taken < 60) score += 40000 * (1 - time_taken / 60);
					transition[0] = false;
					transition[1] = true;
					transition_timestamp = now;
					boss.vx = 0;
					boss.vy = 0;
					boss.base_damage = 20;
					laser_array.is_active = true;
					laser_array.horizontal = true;
					laser_array.vertical = false;
					laser_array.mirror = true;
					laser_array.x = 0;
					laser_array.v = 0.5;
					laser_array.a = 0.1;
					laser_array.v_limit = 1.5;
					laser_array.gap_halfwidth = 250;
					laser_array.hori_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
					laser_array.hori_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
					al_stop_sample(&laser_array_sfx_id);
					al_play_sample(laser_array_sfx, 0.7, 0, 1, ALLEGRO_PLAYMODE_LOOP, &laser_array_sfx_id);
				}
			}

			if (transition[1]) {
				// Phase 2-2: Double Bullet Storm! + Lv. 2 Lasers!

				// Double Bullet Storm
				if ((now - boss.last_shoot_timestamp) / 100000 >= 1.5f) {
					int count = 0;

					if (((int)(now / 100000)) % 2) {
						float vx = -2;
						float vy = 0;
						rotate_vector(&vx, &vy, ((double)(rand() % 15) + 15));

						for (i = 0; count < 20;i++) {
							if (bullets[i].hidden) {
								bullets[i].hidden = false;
								bullets[i].hostile = true;
								bullets[i].img = enemy_bullet_1;
								bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
								bullets[i].x = boss.x - 30;
								bullets[i].y = boss.y + 20;
								bullets[i].vx = -vx;
								bullets[i].vy = -vy;
								bullets[i].base_damage = boss.base_damage;
								rotate_vector(&vx, &vy, 6);
								count++;
							}
						}
					}
					else {
						float vx = 2;
						float vy = 0;
						rotate_vector(&vx, &vy, -((double)(rand() % 15) + 15));

						for (i = 0; count < 20;i++) {
							if (bullets[i].hidden) {
								bullets[i].hidden = false;
								bullets[i].hostile = true;
								bullets[i].img = enemy_bullet_1;
								bullets[i].hitbox_radius = al_get_bitmap_width(bullets[i].img) / 2;
								bullets[i].x = boss.x + 30;
								bullets[i].y = boss.y + 20;
								bullets[i].vx = -vx;
								bullets[i].vy = -vy;
								bullets[i].base_damage = boss.base_damage;
								rotate_vector(&vx, &vy, -6);
								count++;
							}
						}
					}

					boss.last_shoot_timestamp = now;
				}

				// Mirrored Horizontal Laser Array
				laser_array_routine(now);

				// Enter Phase 2-3. But first, move.
				if (boss.current_HP <= 4500) {
					game_log("Entering Phase 2-3.");
					double time_taken = (now - transition_timestamp) / 100000;
					if (time_taken < 60) score += 50000 * (1 - time_taken / 60);
					transition[1] = false;
					transition[2] = true;
					transition_timestamp = now;
					boss.vx = 0;
					boss.vy = 5;
					boss.base_damage = 40;
					laser_array.is_active = false;
					al_stop_sample(&laser_array_sfx_id);
				}

			}

			if (transition[2]) {
				boss.y += boss.vy;
				if (boss.hitbox_radius < 20) boss.hitbox_radius+=0.25;
				if (start_bg_alpha < 200) start_bg_alpha++;
				if (SCREEN_H / 2 < boss.y) {
					boss.y = SCREEN_H / 2;
					transition_timestamp = now;
					transition[2] = false;
					transition[3] = true;
					boss.vx = 0;
					boss.vy = 0;
					thunder_cap = 11;
				}
			}

			if (transition[3]) {
				// Phase 2-3: Lv. 2 Thunder + Circle Wave 1
				if ((now - boss.last_shoot_timestamp) / 100000 > 0.9f) {
					if (point_stack_counter < thunder_cap && boss.current_HP > 0) { // Charge...
						point_stack[++point_stack_counter].x = player.x + 80 - rand() % 160;
						point_stack[point_stack_counter].y = player.y + 80 - rand() % 160;
					}
					else  if (point_stack_counter > -1) { // RELEASE!
						release_lightning = true;
						al_play_sample(thunder_sfx, 1, 0, 2, ALLEGRO_PLAYMODE_ONCE, &thunder_sfx_id);
						float width = al_get_bitmap_width(img_lightning);
						for (i = 0; i <= point_stack_counter; i++) {
							float target_x = point_stack[i].x - width / 2;
							float target_y = point_stack[i].y - width / 2;
							if (pnt_in_rect(player.x, player.y, \
								target_x, target_y, width, \
								width) && \
								(now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
								al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
								game_log("Player takes damage.");
								last_damage_timestamp = now;
								player.current_HP -= 1.5 * boss.base_damage;
								combo = 1; // Reset combo.
								if (player.current_HP <= 0) {
									death_routine();
								}
							}

						}
					}
					game_log("Queued lightning at (%f, %f)", \
						point_stack[point_stack_counter].x, \
						point_stack[point_stack_counter].y);
					boss.last_shoot_timestamp = now;

					double t = (now - transition_timestamp) / 100000;
					float vx = 5;
					float vy = 0;
					rotate_vector(&vx, &vy, 360 * sin(t));
					double count = 0;
					for (i = 0; count < 8; i++) {
						if (bullets[i].hidden){
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							rotate_vector(&vx, &vy, count * 45);
							bullets[i].vx = vx;
							bullets[i].vy = vy;
							bullets[i].x = boss.x;
							bullets[i].y = boss.y;
							bullets[i].base_damage = boss.base_damage / 2;
							count++;
						}
					}
					
					
				}

				// Enter the final phase: 90 Second Overdrive!
				if (boss.current_HP <= 0 && point_stack_counter == -1) {
					double time_taken = (now - transition_timestamp) / 100000;
					if (time_taken < 60) score += 10000 * (1 - time_taken / 60);
					boss.img = img_boss_overdrive;
					transition_timestamp = now;
					transition[3] = false;
					transition[4] = true;
					al_stop_sample(&boss_bgm_A_id);
					//if (!al_play_sample(warning_sfx, 1, 0.5, 1.25, ALLEGRO_PLAYMODE_ONCE, &warning_sfx_id))
						//game_abort("failed to play warning sfx");
					al_play_sample(boss_bgm_B, 1, 0, 1, ALLEGRO_PLAYMODE_LOOP, &boss_bgm_B_id);
				}
			}

			if (transition[4]) {
				if ((now - transition_timestamp) / 100000 > 5.0f) {
					start_scene_phase = 3;
					layer_3_pos_y = -SCREEN_H;
					transition_timestamp = now;
					transition[4] = false;
					transition[0] = true;
					boss.base_damage = 20;
					thunder_cap = 16;
					laser_array.is_active = true;
					laser_array.horizontal = false;
					laser_array.vertical = true;
					laser_array.mirror = true;
					laser_array.y = 0;
					laser_array.v = 0.1;
					laser_array.a = 0;
					laser_array.v_limit = 1.5;
					laser_array.gap_halfwidth = 1;
					laser_array.vert_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
					laser_array.vert_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
					al_play_sample(laser_array_sfx, 0.7, 0, 1, ALLEGRO_PLAYMODE_LOOP, &laser_array_sfx_id);
				}
			}
		}
		if (start_scene_phase == 3) {
			if(!transition[3])
				if ((circle_collision_test(player, boss) || circle_collision_test(octa, boss)) \
					&& (now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
					al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
					game_log("Player takes damage.");
					last_damage_timestamp = now;
					player.current_HP -= 40;
					combo = 1; // Reset combo.
					if (player.current_HP <= 0) {
						death_routine();
					}
				}

			// Phase 3-1: Lv. 2 Laser Array + Lv. 3 Thunder + Circle Wave 2
			if (transition[0]) {
				laser_array_routine(now);

				if ((now - boss.last_shoot_timestamp) / 100000 > 0.85f) {
					if (point_stack_counter < thunder_cap) { // Charge...
						point_stack[++point_stack_counter].x = player.x + 80 - rand() % 160;
						point_stack[point_stack_counter].y = player.y + 80 - rand() % 160;
					}
					else  if (point_stack_counter > -1) { // RELEASE!
						release_lightning = true;
						al_play_sample(thunder_sfx, 1.3, 0, 2, ALLEGRO_PLAYMODE_ONCE, &thunder_sfx_id);
						float width = al_get_bitmap_width(img_lightning);
						for (i = 0; i <= point_stack_counter; i++) {
							float target_x = point_stack[i].x - width / 2;
							float target_y = point_stack[i].y - width / 2;
							if (pnt_in_rect(player.x, player.y, \
								target_x, target_y, width, \
								width) && \
								(now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN) {
								al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
								game_log("Player takes damage.");
								last_damage_timestamp = now;
								player.current_HP -= 2 * boss.base_damage;
								combo = 1; // Reset combo.
								if (player.current_HP <= 0) {
									death_routine();
								}
							}

						}
					}
					game_log("Queued lightning at (%f, %f)", \
						point_stack[point_stack_counter].x, \
						point_stack[point_stack_counter].y);
					boss.last_shoot_timestamp = now;

					double t = (now - transition_timestamp) / 100000;
					float vx = 3.5;
					float vy = 0;
					rotate_vector(&vx, &vy, 360 * sin(t));
					double count = 0;
					for (i = 0; count < 12; i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							rotate_vector(&vx, &vy, count * 30);
							bullets[i].vx = vx;
							bullets[i].vy = vy;
							bullets[i].x = boss.x;
							bullets[i].y = boss.y;
							bullets[i].base_damage = boss.base_damage / 2;
							count++;
						}
					}


				}

				// Enter Phase 3-2.
				if ((now - transition_timestamp) / 100000 > 30.0f) {
					laser_array.is_active = true;
					laser_array.horizontal = true;
					laser_array.vertical = true;
					laser_array.mirror = false;
					laser_array.x = 0;
					laser_array.y = 0;
					laser_array.v = 1.5;
					laser_array.a = 0.1;
					laser_array.v_limit = 2.5;
					laser_array.gap_halfwidth = 150;
					laser_array.vert_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
					laser_array.hori_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
					transition[0] = false;
					transition[1] = true;
					thunder_cap = 10;
					boss.base_damage = 25;
					al_stop_sample(&laser_array_sfx_id);
					al_play_sample(laser_array_sfx, 0.7, 0, 1, ALLEGRO_PLAYMODE_LOOP, &laser_array_sfx_id);
				}
			}

			if (transition[1]) {
				// Phase 3-2: Lv. 3 Laser Array + Lv. 2 Thunder + Circle Wave 3
				laser_array_routine(now);
				if ((now - boss.last_shoot_timestamp) / 100000 > 0.95f) {
					if (point_stack_counter < thunder_cap) { // Charge...
						point_stack[++point_stack_counter].x = player.x + 100 - rand() % 200;
						point_stack[point_stack_counter].y = player.y + 100 - rand() % 200;
					}
					else  if (point_stack_counter > -1) { // RELEASE!
						release_lightning = true;
						al_play_sample(thunder_sfx, 1.2, 0, 2, ALLEGRO_PLAYMODE_ONCE, &thunder_sfx_id);
						float width = al_get_bitmap_width(img_lightning);
						for (i = 0; i <= point_stack_counter; i++) {
							float target_x = point_stack[i].x - width / 2;
							float target_y = point_stack[i].y - width / 2;
							if (pnt_in_rect(player.x, player.y, \
								target_x, target_y, width, \
								width) && \
								(now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
								al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
								game_log("Player takes damage.");
								last_damage_timestamp = now;
								player.current_HP -= 2 * boss.base_damage;
								combo = 1; // Reset combo.
								if (player.current_HP <= 0) {
									death_routine();
								}
							}

						}
					}
					game_log("Queued lightning at (%f, %f)", \
						point_stack[point_stack_counter].x, \
						point_stack[point_stack_counter].y);
					boss.last_shoot_timestamp = now;

					double t = (now - transition_timestamp) / 100000;
					float vx = 2.5;
					float vy = 0;
					rotate_vector(&vx, &vy, 360 * sin(t));
					double count = 0;
					for (i = 0; count < 18; i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							rotate_vector(&vx, &vy, count * 20);
							bullets[i].vx = vx;
							bullets[i].vy = vy;
							bullets[i].x = boss.x;
							bullets[i].y = boss.y;
							bullets[i].base_damage = boss.base_damage / 2;
							count++;
						}
					}

					// Enter Phase 3-3.
					if ((now - transition_timestamp) / 100000 > 60.0f) {
						laser_array.is_active = true;
						laser_array.horizontal = true;
						laser_array.vertical = true;
						laser_array.mirror = true;
						laser_array.x = 0;
						laser_array.y = 0;
						laser_array.v = 1;
						laser_array.a = 0.1;
						laser_array.v_limit = 2.5;
						laser_array.gap_halfwidth = 300;
						laser_array.vert_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
						laser_array.vert_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
						laser_array.hori_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
						laser_array.hori_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
						thunder_cap = 19;
						boss.base_damage = 30;
						al_stop_sample(&laser_array_sfx_id);
						al_play_sample(laser_array_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_LOOP, &laser_array_sfx_id);
						transition[1] = false;
						transition[2] = true;
					}
				}
			}

			if (transition[2]) {
				// Phase 3-3: "Stormbound" (Lv. 4 Thunder + Laser Array + Circle Wave 4)
				laser_array_routine(now);
				if ((now - boss.last_shoot_timestamp) / 100000 > 0.85f) {
					if (point_stack_counter < thunder_cap) { // Charge...
						point_stack[++point_stack_counter].x = player.x + 100 - rand() % 200;
						point_stack[point_stack_counter].y = player.y + 100 - rand() % 200;
					}
					else  if (point_stack_counter > -1) { // RELEASE!
						release_lightning = true;
						al_play_sample(thunder_sfx, 1.4, 0, 2, ALLEGRO_PLAYMODE_ONCE, &thunder_sfx_id);
						float width = al_get_bitmap_width(img_lightning);
						for (i = 0; i <= point_stack_counter; i++) {
							float target_x = point_stack[i].x - width / 2;
							float target_y = point_stack[i].y - width / 2;
							if (pnt_in_rect(player.x, player.y, \
								target_x, target_y, width, \
								width) && \
								(now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
								al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
								game_log("Player takes damage.");
								last_damage_timestamp = now;
								player.current_HP -= 2 * boss.base_damage;
								combo = 1; // Reset combo.
								if (player.current_HP <= 0) {
									death_routine();
								}
							}

						}
					}
					game_log("Queued lightning at (%f, %f)", \
						point_stack[point_stack_counter].x, \
						point_stack[point_stack_counter].y);
					boss.last_shoot_timestamp = now;

					double t = (now - transition_timestamp) / 100000;
					float vx = 1.0;
					float vy = 0;
					rotate_vector(&vx, &vy, 360 * sin(t));
					double count = 0;
					for (i = 0; count < 18; i++) {
						if (bullets[i].hidden) {
							bullets[i].hidden = false;
							bullets[i].hostile = true;
							bullets[i].img = enemy_bullet_1;
							rotate_vector(&vx, &vy, count * 20);
							bullets[i].vx = vx;
							bullets[i].vy = vy;
							bullets[i].x = boss.x;
							bullets[i].y = boss.y;
							bullets[i].base_damage = boss.base_damage / 2;
							count++;
						}
					}

					// Ending Transition
					if ((now - transition_timestamp) / 100000 > 90.0f) {
						al_play_sample(death_sfx, 1.2, 0, 1, ALLEGRO_PLAYMODE_ONCE, &death_sfx_id);
						point_stack_counter = -1;
						laser_array.is_active = false;
						al_stop_sample(&laser_array_sfx_id);
						score += 1000000 * lives;
						transition[2] = false;
						transition[3] = true;
					}
				}
			}

			if (transition[3]) {
				if ((now - transition_timestamp) / 100000 > 100.0f) {
					al_stop_sample(&boss_bgm_B_id);
					transition[3] = false;
					transition_target = SCENE_RESULTS;
					game_change_scene(SCENE_TRANSITION);
				}
			}
		}
		

	}
	else if (active_scene == SCENE_TRANSITION) {
		if (!fade_out) {
			game_change_scene(transition_target);
		}
	}
}

void game_draw(void) {
	// Main processes of game_draw()
	if (active_scene == SCENE_MENU) {
		draw_animation(main_img_background_anim, 0, 0, 0);
		al_draw_text(font_OpenSans_24, al_map_rgb(255, 255, 255), SCREEN_W - 20, SCREEN_H - 50, ALLEGRO_ALIGN_RIGHT, "Press enter key to start");
		al_draw_text(font_OpenSans_24, al_map_rgb(150, 150, 150), 20, 25, 0, "Press Esc key to quit");
		// Draw settings images.
		if (pnt_in_rect(mouse_x, mouse_y, SCREEN_W - 48, 10, 38, 38)){
			al_draw_bitmap(img_settings2, SCREEN_W - 48, 10, 0);
		}
		else {
			al_draw_bitmap(img_settings, SCREEN_W - 48, 10, 0);
		}
	} else if (active_scene == SCENE_START) {
		int i;
		double now = al_get_timer_count(ingame_timer);

		// Controls scrolling.
		if (bg_pos_y >= 0) {
			bg_pos_y -= SCREEN_H;
		}
		// NOTE: For smooth transition into a different background, switch the background for this line first.
		al_draw_bitmap(start_img_background, 0, bg_pos_y, 0);

		al_draw_bitmap(start_img_background, 0, bg_pos_y + SCREEN_H, 0);

		// Bottom Shade
		al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(start_bg_alpha, start_bg_alpha, start_bg_alpha, start_bg_alpha));
	
		
		// Cloud Layer 1
		if (start_scene_phase > 0) {
			if (layer_1_pos_y >= 0) {
				layer_1_pos_y -= SCREEN_H;
			}
			al_draw_bitmap(cloud_layer_1, 0, layer_1_pos_y, 0);
			al_draw_bitmap(cloud_layer_1, 0, layer_1_pos_y + SCREEN_H, 0);
		}

		// Cloud Layer 2
		if (start_scene_phase > 1) {
			if (layer_2_pos_y >= 0) {
				layer_2_pos_y -= SCREEN_H;
			}
			al_draw_bitmap(cloud_layer_2, 0, layer_2_pos_y, 0);
			al_draw_bitmap(cloud_layer_2, 0, layer_2_pos_y + SCREEN_H, 0);
		}

		// Cloud Layer 3
		if (start_scene_phase > 2) {
			if (layer_3_pos_y >= 0) {
				layer_3_pos_y -= SCREEN_H;
			}
			al_draw_bitmap(cloud_layer_3, 0, layer_3_pos_y, 0);
			al_draw_bitmap(cloud_layer_3, 0, layer_3_pos_y + SCREEN_H, 0);
		}

		if (!release_lightning) {
			double t = (now - transition_timestamp) / 100000;
			float y_offset = 25;
			for (i = 0; i <= point_stack_counter; i++) {
				float x = point_stack[i].x;
				float y = point_stack[i].y;
				float w = al_get_bitmap_width(img_lightning);
				float ow = al_get_bitmap_width(img_warning_label);
				al_draw_scaled_bitmap(img_warning_label, \
					0, 0, ow, ow, \
					x - w / 2, y - w / 2, w, w, 0);
				int k = 0;
				if (point_stack_counter >= thunder_cap - 1) k = 190;
				else if (point_stack_counter == thunder_cap - 2) k = 140;
				else if (point_stack_counter == thunder_cap - 3) k = 90;
				al_draw_filled_rectangle(x - w / 2, y - w / 2, x + w / 2, \
					y + w / 2, al_map_rgba(10 + k, 10, 10, 25+25*sin(PI*t)));
			}
		}

		// Draw all movable objects if applicable.

		// Draw boss.
		if (start_scene_phase > 0) {
			float boss_hp_percentage = boss.current_HP / boss.max_HP;
			if (start_scene_phase == 3 && transition[3]) {
				draw_movable_object(boss, 3, 0);
				if (boss.alpha > 0) boss.alpha-=3;
			}
			else {
				draw_movable_object(boss, 0, 0);
			}
			// Circle to represent boss HP and hitbox.
			if(start_scene_phase < 3)
				al_draw_circle(boss.x, boss.y, boss.hitbox_radius, \
				al_map_rgba(255*(1- boss_hp_percentage), \
					255*boss_hp_percentage, 0, 100), 10);
			else {
				if (!transition[3]) {
					al_draw_circle(boss.x, boss.y, boss.hitbox_radius, \
					al_map_rgba(200, 60, 0, 50), 10);
					al_draw_arc(boss.x, boss.y, boss.hitbox_radius, 0, 2 * PI * (1 - (now - transition_timestamp) / 100000 / 90.0f), al_map_rgba(255, 0, 0, 100), 10);
				}
			}
		}
		
		for (i = 0; i < MAX_ENEMY; i++)
			draw_movable_object(enemies[i], 1, 0);
		if (!dying) {
			player.alpha = 255;
			if (player_invulnerable) draw_movable_object(player, 2, 0);
			else draw_movable_object(player, 0, 0);

			draw_movable_object(player, 1, 0);

			if (player_invulnerable) draw_movable_object(octa, 2, 0);
			else draw_movable_object(octa, 0, 0);
		}
		else {
			draw_movable_object(player, 3, 0);
			if(player.alpha > 0) player.alpha-=5;
		}
		
		if (is_synchronizing)
			draw_dotted_line(player.x + player.hitbox_source_x_offset, player.y + player.hitbox_source_y_offset, octa.x + octa.hitbox_source_x_offset, octa.y + octa.hitbox_source_y_offset, al_map_rgb(150, 255, 150));

		// Draw Laser Array
		if (laser_array.is_active) {
			if(laser_array.vertical){

				// debug code: highlight collision box.
				/*
				al_draw_filled_rectangle(-player.hitbox_radius, \
					laser_array.y - SCREEN_H / 16 - \
					player.hitbox_radius, \
					-player.hitbox_radius+laser_array.vert_gap_pos - laser_array.gap_halfwidth + \
					player.hitbox_radius * 2, laser_array.y - SCREEN_H / 16 - \
					player.hitbox_radius + SCREEN_H / 16 + \
					player.hitbox_radius * 2, al_map_rgb(255, 0, 0));
				al_draw_filled_rectangle(laser_array.vert_gap_pos + \
					laser_array.gap_halfwidth - player.hitbox_radius, \
					laser_array.y - SCREEN_H / 16 - \
					player.hitbox_radius, \
					laser_array.vert_gap_pos + \
					laser_array.gap_halfwidth - player.hitbox_radius+SCREEN_W + player.hitbox_radius * 2, \
					laser_array.y - SCREEN_H / 16 - \
					player.hitbox_radius+SCREEN_H / 16 + player.hitbox_radius * 2, al_map_rgb(0, 0, 255));
				*/
				al_draw_bitmap_region(img_laser_array_horizontal, 0, 0, \
					laser_array.vert_gap_pos - laser_array.gap_halfwidth, \
					SCREEN_H / 16, 0, laser_array.y - SCREEN_H / 16, 0);
				al_draw_bitmap_region(img_laser_array_horizontal, 0, 0, \
					SCREEN_W - laser_array.vert_gap_pos - laser_array.gap_halfwidth, \
					SCREEN_H / 16, laser_array.vert_gap_pos + laser_array.gap_halfwidth, \
					laser_array.y - SCREEN_H / 16, 0);
				if (laser_array.mirror) {
					float mirror_y = SCREEN_W - laser_array.y;
					al_draw_bitmap_region(img_laser_array_horizontal, 0, 0, \
						laser_array.vert_gap_pos_mirror - laser_array.gap_halfwidth, \
						SCREEN_H / 16, 0, mirror_y - SCREEN_H / 16, 0);
					al_draw_bitmap_region(img_laser_array_horizontal, 0, 0, \
						SCREEN_W - laser_array.vert_gap_pos_mirror - laser_array.gap_halfwidth, \
						SCREEN_H / 16, laser_array.vert_gap_pos_mirror + laser_array.gap_halfwidth, \
						mirror_y - SCREEN_H / 16, 0);

				}
				
			}

			if (laser_array.horizontal) {
				al_draw_bitmap_region(img_laser_array, 0, 0, \
					SCREEN_W / 16, laser_array.hori_gap_pos - laser_array.gap_halfwidth, \
					laser_array.x - SCREEN_W / 16, 0, 0);
				al_draw_bitmap_region(img_laser_array, 0, 0, \
					SCREEN_W / 16, SCREEN_H - laser_array.hori_gap_pos - laser_array.gap_halfwidth, \
					laser_array.x - SCREEN_W / 16, laser_array.hori_gap_pos + laser_array.gap_halfwidth, 0);
				if (laser_array.mirror) {
					float mirror_x = SCREEN_W - laser_array.x;
					al_draw_bitmap_region(img_laser_array, 0, 0, \
						SCREEN_W / 16, laser_array.hori_gap_pos_mirror - laser_array.gap_halfwidth, \
						mirror_x - SCREEN_W / 16, 0, 0);
					al_draw_bitmap_region(img_laser_array, 0, 0, \
						SCREEN_W / 16, SCREEN_H - laser_array.hori_gap_pos_mirror - laser_array.gap_halfwidth, \
						mirror_x - SCREEN_W / 16, laser_array.hori_gap_pos_mirror + laser_array.gap_halfwidth, 0);
				}
			}
		}

		for (i = 0; i < MAX_BULLET; i++)
			draw_movable_object(bullets[i], 0, 0);

		if (release_lightning) {
			float y_offset = 95;
			float t = (now - boss.last_shoot_timestamp) / 100000;
			if (t < 0.8f) {
				for (i = 0; i <= point_stack_counter; i++) {
					float x = point_stack[i].x;
					float y = point_stack[i].y;
					float w = al_get_bitmap_width(img_lightning);
					float h = al_get_bitmap_height(img_lightning);
					al_draw_tinted_bitmap(img_lightning, \
						al_map_rgba(255, 255, 255, 255*(1-t/0.8)), x - w / 2, y - h/2 - y_offset, 0);
				}
			}
			else {
				point_stack_counter = -1;
				release_lightning = false;
			}
			
		}
		

		if (in_focus && !dying) {
			draw_circle_hitbox(player, player.hitbox_radius);
		}

		if (dying) {
			draw_expanding_fading_circle(respawn_x, respawn_y, 16, 255, 0, 0, &dying_alpha, -2, &dying, true);
			if(lives>0) al_draw_text(font_pirulen_32, al_map_rgb(255, 100, 100), SCREEN_W / 2, SCREEN_H / 2, ALLEGRO_ALIGN_CENTER, "PLAYER DOWN");
			else al_draw_text(font_pirulen_32, al_map_rgb(255, 100, 100), SCREEN_W / 2, 30, ALLEGRO_ALIGN_CENTER, "DEFEATED");
		}
		else if (respawning) {
			draw_expanding_fading_circle(respawn_x, respawn_y, 16, 200, 200, 200, &respawning_alpha, -4, &respawning, false);
			al_draw_text(font_pirulen_32, al_map_rgb(100, 100, 255), SCREEN_W / 2, SCREEN_H / 2, ALLEGRO_ALIGN_CENTER, "RESPAWNING");
		}

		// Ult handling.
		if (ult_active) {
			if (sync_percentage != 1.0f) {
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius, al_map_rgba(0, 40, 220, 255*(1 - (now - ult_trigger_timestamp)/100000/2.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 1.414, al_map_rgba(0, 80, 220, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 2.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 2, al_map_rgba(0, 120, 220, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 2.0f)), 15);
			}
			else {
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius, al_map_rgba(80, 160, 160, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 3.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 1.414, al_map_rgba(100, 140, 255, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 3.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 2, al_map_rgba(120, 120, 255, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 3.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 2.586, al_map_rgba(140, 100, 255, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 3.0f)), 15);
				al_draw_circle(ult_hitbox.x, ult_hitbox.y, ult_hitbox.hitbox_radius / 3, al_map_rgba(160, 80, 255, 255 * (1 - (now - ult_trigger_timestamp) / 100000 / 3.0f)), 15);
			}
		}


		if (game_paused) {
			al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 50));
			al_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 25), 50);
			al_draw_text(font_pirulen_32, al_map_rgb(255, 255, 255), SCREEN_W / 2, SCREEN_H / 2, ALLEGRO_ALIGN_CENTER, "P A U S E D");
			al_draw_filled_rectangle(100, 550, 700, 650, al_map_rgba(100, 100, 100, 100));
			if (pnt_in_rect(mouse_x, mouse_y, 100, 550, 600, 100)) {
				al_draw_filled_rectangle(100, 550, 700, 650, al_map_rgba(125, 125, 125, 100));
			}
			else al_draw_filled_rectangle(100, 550, 700, 650, al_map_rgba(100, 100, 100, 100));
			al_draw_text(font_pirulen_32, al_map_rgb(0, 0, 0), SCREEN_W / 2, 580, ALLEGRO_ALIGN_CENTER, "RETURN TO MAIN MENU");
		}

		// Render animated elements of the player UI. (meters, etc.)
		// Render the UI container.
		if (player.y < SCREEN_H - 150) {
			draw_player_UI_elements();
			al_draw_bitmap(player_UI_container, 0, SCREEN_H - 100, 0);
			
		}
		
		// Boss WARNING.
		if(start_scene_phase == 0){		
			if (transition[0]) {
				double t = (now - transition_timestamp) / 100000;
				al_draw_line(0, SCREEN_H / 2 - 60, SCREEN_W * t / 1.0f, SCREEN_H / 2 - 60, al_map_rgba(255, 0, 0, 255 * sin(PI * t)),10);
				al_draw_line(SCREEN_W - SCREEN_W * t / 1.0f, SCREEN_H / 2 + 60, SCREEN_W, SCREEN_H / 2 + 60, al_map_rgba(255, 0, 0, 255 * sin(PI * t)), 10);
				al_draw_text(font_impacted, al_map_rgba(255, 0, 0, 255 * sin(PI * t)), SCREEN_W / 2, SCREEN_H / 2 - 75, ALLEGRO_ALIGN_CENTER, "WARNING");
			}
		}
		else if (start_scene_phase == 2) {
			if (transition[4]) {
				double t = (now - transition_timestamp) / 100000;
				al_draw_line(0, SCREEN_H / 2 - 60, SCREEN_W* t / 1.0f, SCREEN_H / 2 - 60, al_map_rgba(255, 0, 0, 255 * sin(PI * t)), 10);
				al_draw_line(SCREEN_W - SCREEN_W * t / 1.0f, SCREEN_H / 2 + 60, SCREEN_W, SCREEN_H / 2 + 60, al_map_rgba(255, 0, 0, 255 * sin(PI * t)), 10);
				al_draw_text(font_impacted, al_map_rgba(255, 51, 0, 200 * sin(PI * t + PI/2)), SCREEN_W / 2, SCREEN_H / 2 - 75, ALLEGRO_ALIGN_CENTER, "WARNING");
				al_draw_text(font_impacted, al_map_rgba(255, 100, 0, 200 * sin(PI * t)), SCREEN_W / 2, SCREEN_H / 2 - 185, ALLEGRO_ALIGN_CENTER, "90 SEC OVERDRIVE");
				al_draw_text(font_impacted, al_map_rgba(255, 100, 0, 200 * sin(PI * t + PI/2)), SCREEN_W / 2, SCREEN_H / 2 + 35, ALLEGRO_ALIGN_CENTER, "90 SEC OVERDRIVE");
			}
		}


		// TODO:
		// 1) Render image props for dialogues.
		// 2) Render message boxes and prompts.

	}
	else if (active_scene == SCENE_SETTINGS) {
		al_clear_to_color(al_map_rgb(0, 0, 0));
		al_draw_text(font_OpenSans_32, al_map_rgb(150, 200, 150), SCREEN_W / 2, 30, ALLEGRO_ALIGN_CENTER, "Settings");
		if (pnt_in_rect(mouse_x, mouse_y, SCREEN_W - 48, 10, 38, 38))
			al_draw_bitmap(img_settings2, SCREEN_W - 48, 10, 0);
		else
			al_draw_bitmap(img_settings, SCREEN_W - 48, 10, 0);
		if (pnt_in_rect(mouse_x, mouse_y, SCREEN_W / 8, 85, SCREEN_W * 3 / 4, 50)) {
			al_draw_filled_rectangle(SCREEN_W / 8, 85, SCREEN_W * 7 / 8, 135, al_map_rgb(100, 100, 100));
		}
		al_draw_rectangle(SCREEN_W / 8, 85, SCREEN_W * 7 / 8, 135, al_map_rgb(0, 0, 50), 5);
		al_draw_text(font_OpenSans_24, al_map_rgb(150, 200, 150), SCREEN_W / 2, 90, ALLEGRO_ALIGN_CENTER, \
			"CLEAR SCOREBOARD");
	}
	else if (active_scene == SCENE_RESULTS) {
		al_clear_to_color(al_map_rgb(0, 0, 0));
		float x_pos = (-al_get_bitmap_width(results_img_background) + \
			SCREEN_W) / 2;
		float y_pos = (-al_get_bitmap_height(results_img_background) + \
			SCREEN_H) / 2;
		al_draw_bitmap(results_img_background, x_pos, y_pos, 0);
		al_draw_textf(font_OpenSans_32, al_map_rgb(0, 0, 0), \
			SCREEN_W / 2, 64, ALLEGRO_ALIGN_CENTER, "FINAL SCORE: %03d%03d%03d", score / 1000000, \
			(score % 1000000 - score % 1000) / 1000, score % 1000);
		al_draw_multiline_textf(font_OpenSans_24, al_map_rgb(0, 0, 0), \
			SCREEN_W / 2, 120, SCREEN_W, 40, ALLEGRO_ALIGN_CENTER, \
			"SCOREBOARD\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n", \
		score_list[0], score_list[1], score_list[2], score_list[3], \
		score_list[4], score_list[5], score_list[6], score_list[7], \
		score_list[8], score_list[9], score_list[10]);
		
	}
	else if (active_scene == SCENE_GAME_OVER) {
		al_clear_to_color(al_map_rgb(255, 255, 255));
		al_draw_bitmap(game_over_img_background, 0, 0, 0);
		al_draw_text(font_OpenSans_24, al_map_rgb(255, 100, 100), 5\
			, SCREEN_H - 50, 0, "Press enter key to go back to main menu.");
	}
	else if (active_scene == SCENE_TITLECARD) {
		al_draw_bitmap(img_titlecard, 0, 0, 0);
	}
	

	
	// Fade routine. Carried out last so that it's on the topmost layer.
	if (fade_out) {
		al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(fade_r, fade_g, fade_b, fade_current_alpha));
		if (fade_target_alpha - fade_current_alpha < fade_rate) {
			fade_current_alpha = fade_target_alpha;
		}
		else fade_current_alpha += fade_rate;

		if (fade_current_alpha >= fade_target_alpha) {
			fade_out = false;
		}
	}
	else if (fade_in) {
		al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(fade_r, fade_g, fade_b, fade_current_alpha));
		if (fade_current_alpha >= fade_rate) {
			fade_current_alpha -= fade_rate;
		}
		else {
			fade_current_alpha = 0;
		}
		if (fade_current_alpha <= fade_target_alpha) {
			fade_in = false;
		}
	}
	

	

	al_flip_display();
}

void game_destroy(void) {
	// Destroy everything you have created.
	// Free the memories allocated by malloc or allegro functions.
	// Destroy shared resources.
	al_destroy_font(font_OpenSans_32);
	al_destroy_font(font_OpenSans_24);
	al_destroy_font(font_pirulen_32);
	al_destroy_font(font_pirulen_24);
	al_destroy_sample(button_click_sfx);

	/* Menu Scene resources*/
	al_destroy_bitmap(main_img_background_anim.sheet);
	al_destroy_sample(main_bgm);
	al_destroy_bitmap(img_settings);
	al_destroy_bitmap(img_settings2);

	/* Start Scene resources*/
	al_destroy_bitmap(start_img_background);
	al_destroy_bitmap(cloud_layer_1);
	al_destroy_bitmap(cloud_layer_2);
	al_destroy_bitmap(cloud_layer_3);
	al_destroy_bitmap(start_img_player);
	al_destroy_bitmap(player_animation_default);
	al_destroy_sample(player_damaged_sfx);
	al_destroy_sample(death_sfx);

	al_destroy_bitmap(img_boss);
	al_destroy_bitmap(img_boss_overdrive);
	al_destroy_bitmap(img_lightning);
	al_destroy_bitmap(img_warning_label);
	al_destroy_sample(thunder_sfx);

	al_destroy_bitmap(img_octa);
	al_destroy_bitmap(enemy_animation_default);
	al_destroy_sample(enemy_death_sfx);

	al_destroy_sample(upgrade_sfx);

	al_destroy_bitmap(player_UI_container);
	al_destroy_bitmap(boss_UI_container);
	int j;
	for (j = 0;j < 1;j++) {
		al_destroy_bitmap(enemy_animation_array[j].sheet);
	}
	al_destroy_sample(start_bgm);
	al_destroy_sample(boss_bgm_A);
	al_destroy_sample(boss_bgm_B);
	al_destroy_sample(warning_sfx);
	fclose(script);

	al_destroy_bitmap(img_laser_array);
	al_destroy_bitmap(img_laser_array_horizontal);
	al_destroy_sample(laser_array_sfx);

	al_destroy_bitmap(img_bullet);
	al_destroy_sample(player_laser_sfx);
	al_destroy_bitmap(octa_charge_shot);
	al_destroy_sample(octa_charge_shot_sfx);

	al_destroy_bitmap(enemy_bullet_1);

	al_destroy_sample(small_explosion_sfx);
	al_destroy_sample(ult_sfx_A);
	al_destroy_sample(ult_sfx_B);

	/* Results Scene resources*/
	al_destroy_bitmap(results_img_background);
	if (scoreboard) fclose(scoreboard);

	/* Game Over Scene resources*/
	al_destroy_bitmap(game_over_img_background);
	al_destroy_sample(game_over_bgm);

	/* Titlecard resources*/
	al_destroy_bitmap(img_titlecard);

	al_destroy_timer(game_update_timer);
	al_destroy_event_queue(game_event_queue);
	al_destroy_display(game_display);
	free(mouse_state);
}

void game_change_scene(int next_scene) {
	game_log("Change scene from %d to %d", active_scene, next_scene);
	// TODO: Destroy resources initialized when creating scene.
	if (active_scene == SCENE_MENU) {
		al_stop_sample(&main_bgm_id);
		game_log("stop audio (bgm)");
	} else if (active_scene == SCENE_START) {
		al_stop_sample(&start_bgm_id);
		game_log("stop audio (bgm)");
		al_stop_sample(&boss_bgm_A_id);
		al_stop_sample(&boss_bgm_B_id);
		al_stop_sample(&warning_sfx_id);
		al_stop_sample(&laser_array_sfx_id);
		al_stop_sample(&thunder_sfx_id);

		// Hide all objects, terminating their processes regardless of condition.
		int i;
		for (i = 0; i < MAX_BULLET; i++) 
			bullets[i].hidden = true;
		
		for (i = 0; i < MAX_ENEMY; i++)
			enemies[i].hidden = true;

		
	} else if (active_scene == SCENE_GAME_OVER){
		al_stop_sample(&game_over_bgm_id);
		game_log("stop audio (bgm)");
	}
	else if (active_scene == SCENE_RESULTS) {
		al_stop_sample(&game_over_bgm_id);
		game_log("stop audio (bgm)");
	} else if (active_scene == SCENE_TRANSITION) {
		fade_out = false;
		// Fade in from black.
		fade_in = true;
		fade_r = 0;
		fade_g = 0;
		fade_b = 0;
		fade_current_alpha = 255;
		fade_target_alpha = 0;
		fade_rate = 4;
	}
	active_scene = next_scene;
	// TODO: Allocate resources before entering scene.
	if (active_scene == SCENE_MENU) {
		if (!al_play_sample(main_bgm, 1, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, &main_bgm_id))
			game_abort("failed to play audio (bgm)");
	} else if (active_scene == SCENE_START) {
		int i;
		game_paused = false;
		point_stack_counter = -1;
		release_lightning = false;
		laser_array.is_active = false;
		start_scene_phase = 0;
		start_bg_alpha = 0;
		// Initialize transition booleans as false:
		for (i = 0; i < TRANSITION_SEGS; i++) {
			transition[i] = false;
		}

		active_enemy_count = 0;
		lives = 5;
		score = 0;
		sync_percentage = 0;
		sync_rate = 0;
		is_synchronizing = false;
		is_octa_dormant = true;
		dying = false;
		respawning = false;
		ult_active = false;
		

		// Initialize background scroll position.
		bg_pos_y = -SCREEN_H;

		

		// Initialize player parameters.
		player.img = start_img_player;
		player.animations[0].sheet = player_animation_default;
		player.animations[0].cell_rows = 1;
		player.animations[0].cell_columns = 6;
		player.animations[0].cell_height = 192;
		player.animations[0].cell_width = 192;
		player.animations[0].last_col = 6;
		player.animations[0].current_row = 0;
		player.animations[0].current_column = 0;
		player.animations[0].frame_repeat = 0;
		player.animations[0].dupe_frames = 5;
		player.alpha = 255;
		player_attack_level = 0;

		last_damage_timestamp = 0;
		last_release_charge_timestamp = 0;
		last_unsync_timestamp = 0;
		last_octa_switch_timestamp = 0;

		player.x = 400;
		player.y = 500;
		player.w = player.animations[0].cell_width;
		player.h = player.animations[0].cell_height;
		player.current_HP = 100;
		player.max_HP = 100;
		player.hitbox_radius = 8;
		player.hitbox_source_x_offset = 0;
		player.hitbox_source_y_offset = -21;

		octa.img = img_octa;
		octa.x = player.x;
		octa.y = player.y;
		octa.w = al_get_bitmap_width(octa.img);
		octa.h = al_get_bitmap_height(octa.img);
		octa.hitbox_radius = 8;
		octa.hitbox_source_x_offset = 0;
		octa.hitbox_source_y_offset = -21;



		// Initialize enemy basic parameters.
		for (i = 0; i < MAX_ENEMY; i++) {
			// Initialize default animation parameters.
			enemies[i].animations[0].sheet = enemy_animation_default;
			enemies[i].animations[0].cell_rows = 1;
			enemies[i].animations[0].cell_columns = 4;
			enemies[i].animations[0].cell_height = 64;
			enemies[i].animations[0].cell_width = 64;
			enemies[i].animations[0].last_col = 4;
			enemies[i].animations[0].current_row = 0;
			enemies[i].animations[0].current_column = 0;
			enemies[i].animations[0].frame_repeat = 0;
			enemies[i].animations[0].dupe_frames = 5;

			enemies[i].w = enemies[i].animations[0].cell_width;
			enemies[i].h = enemies[i].animations[0].cell_height;
			enemies[i].hitbox_radius = 32;
			enemies[i].hitbox_source_x_offset = 0;
			enemies[i].hitbox_source_y_offset = 0;
			enemies[i].hidden = true;
			enemies[i].last_shoot_timestamp = 0;
		}
		// Initialize bullets.
		for (i = 0; i < MAX_BULLET; i++) {
			bullets[i].w = al_get_bitmap_width(img_bullet);
			bullets[i].h = al_get_bitmap_height(img_bullet);
			bullets[i].img = img_bullet;
			bullets[i].hidden = true;
			bullets[i].hostile = false;
			bullets[i].hitbox_source_x_offset = 0;
			bullets[i].hitbox_source_y_offset = 0;
		}
		
		last_shoot_timestamp = 0;

		al_start_timer(ingame_timer);
		al_set_timer_count(ingame_timer, 0);

		if (fscanf(script, "%lf", &next_event_timestamp) != 1) {
			game_abort("Empty Script.");
		}
		else {
			script_done = false;
			game_log("Next Event Timestamp: %lf", next_event_timestamp);
		}

		if (!al_play_sample(start_bgm, 1, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, &start_bgm_id))
			game_abort("failed to play audio (bgm)");
	} else if (active_scene == SCENE_GAME_OVER) {
		if (!al_play_sample(game_over_bgm, 1, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, &game_over_bgm_id))
			game_abort("failed to play audio (bgm)");
		// Reload script
		fclose(script);
		script = fopen("level_script.txt", "r");
		if (!script)
			game_abort("failed to load level script");
		
		al_stop_timer(ingame_timer);
	} else if (active_scene == SCENE_RESULTS){
		if (!al_play_sample(game_over_bgm, 1, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, &game_over_bgm_id))
			game_abort("failed to play audio (bgm)");
		// Reload script
		fclose(script);
		script = fopen("level_script.txt", "r");
		if (!script)
			game_abort("failed to load level script");

		// Load scoreboard.
		scoreboard = fopen("scoreboard.txt", "r");
		if (!scoreboard)
			game_abort("failed to load scoreboard");
		int i;
		int count = 0;
		int flag = 1;
		for (i = 0; i < 10; i++) {
			int k = fscanf(scoreboard, "%d", &score_list[i]);
			if (k != 1) game_abort("failed to parse score at %d", i);
			
		}
		score_list[10] = score;
		qsort(score_list, 11, sizeof(int), compare_int);
		fclose(scoreboard);
		scoreboard = fopen("scoreboard.txt", "w");
		for (i = 0; i < 10; i++) {
			fprintf(scoreboard, "%d\n", score_list[i]);
		}
		fclose(scoreboard);
		pause_time_record = al_get_timer_count(ingame_timer) / 100000;
		al_stop_timer(ingame_timer);
	} else if (active_scene == SCENE_TRANSITION) {
		
		// Set up fade variables for game_draw() to begin fading.
		fade_out = true;
		fade_r = 0;
		fade_g = 0;
		fade_b = 0;
		fade_current_alpha = 0;
		fade_target_alpha = 255;
		fade_rate = 4;
	}
}

void on_key_down(int keycode) {
	double now = al_get_timer_count(ingame_timer);

	if (active_scene == SCENE_MENU) {
		if (keycode == ALLEGRO_KEY_ENTER){
			transition_target = SCENE_START;
			game_change_scene(SCENE_TRANSITION);
		}
		else if (keycode == ALLEGRO_KEY_ESCAPE) {
			transition_target = SCENE_EXIT;
			game_change_scene(SCENE_TRANSITION);
		}

	}
	else if (active_scene == SCENE_START) {
		if (keycode == ALLEGRO_KEY_ESCAPE && !game_paused) {
			game_paused = true;
			pause_time_record = al_get_timer_count(ingame_timer);
			al_stop_timer(ingame_timer);
			game_log("Game paused.");
		}
		else if (keycode == ALLEGRO_KEY_ESCAPE && game_paused){
			game_paused = false;
			al_start_timer(ingame_timer);
			al_set_timer_count(ingame_timer, pause_time_record);
			game_log("Game resumed.");
		}
		else if (keycode == ALLEGRO_KEY_LSHIFT) {
			in_focus = true;
			game_log("Enter focus mode.");
		}
		else if (keycode == ALLEGRO_KEY_Z && !ult_active && sync_percentage >= 0.5f) {
			if (sync_percentage == 1.0f) al_play_sample(ult_sfx_B, 1, 0, 1.2, ALLEGRO_PLAYMODE_ONCE, &ult_sfx_B_id);
			else al_play_sample(ult_sfx_A, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &ult_sfx_A_id);
			ult_active = true;
			boss_damaged_by_ult = false;
			ult_hitbox.hitbox_source_x_offset = 0;
			ult_hitbox.hitbox_source_y_offset = 0;
			ult_hitbox.x = player.x;
			ult_hitbox.y = player.y;
			ult_hitbox.hitbox_radius = 0;
			ult_trigger_timestamp = now;
			game_log("Using ult.");
		}
	}
	else if (active_scene == SCENE_GAME_OVER || active_scene == SCENE_RESULTS || active_scene == SCENE_TITLECARD) {
		if (keycode == ALLEGRO_KEY_ENTER){
			transition_target = SCENE_MENU;
			game_change_scene(SCENE_TRANSITION);
		}
	}
}

void on_key_up(int keycode) {
	if (active_scene == SCENE_START) {
		if (keycode == ALLEGRO_KEY_LSHIFT) {
			in_focus = false;
			game_log("Exit focus mode.");
		}
	}
}

void on_mouse_down(int btn, int x, int y) {
	// When settings clicked, switch to settings scene.
	if (active_scene == SCENE_MENU) {
		if (btn == 1) { //1: LEFT CLICK
			if (pnt_in_rect(x, y, SCREEN_W - 48, 10, 38, 38)){
				al_play_sample(button_click_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &button_click_sfx_id);
				game_change_scene(SCENE_SETTINGS);
			}
		}
	}
	else if (active_scene == SCENE_START) {
		if (btn == 1 && game_paused) {
			if (pnt_in_rect(x, y, 100, 550, 600, 100)) {
				al_play_sample(button_click_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &button_click_sfx_id);
				// Reload script
				fclose(script);
				script = fopen("level_script.txt", "r");
				if (!script)
					game_abort("failed to load level script");
				al_stop_timer(ingame_timer);
				transition_target = SCENE_MENU;
				game_change_scene(SCENE_TRANSITION);
			}
		}
	}
	else if (active_scene == SCENE_SETTINGS) {
		if (btn == 1) { //1: LEFT CLICK
			if (pnt_in_rect(x, y, SCREEN_W - 48, 10, 38, 38)) {
				al_play_sample(button_click_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &button_click_sfx_id);
				game_change_scene(SCENE_MENU);
			}
			else if (pnt_in_rect(x, y, SCREEN_W / 8, 85, SCREEN_W * 3 / 4, 50)) {
				al_play_sample(button_click_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &button_click_sfx_id);
				scoreboard = fopen("scoreboard.txt", "w");
				if (!scoreboard)
					game_abort("failed to load scoreboard");
				fprintf(scoreboard, "0\n0\n0\n0\n0\n0\n0\n0\n0\n0\n");
				fclose(scoreboard);
			}
		}
	}
}

void draw_movable_object(MovableObject obj, int flag, int id) {
	if (obj.hidden)
		return;
	if(flag == 0)
		al_draw_bitmap(obj.img, round((double)obj.x - (double)obj.w / 2), round((double)obj.y - (double)obj.h / 2), 0);
	else if (flag == 1){
		draw_animation(obj.animations[id], round((double)obj.x - (double)obj.w / 2), round((double)obj.y - (double)obj.h / 2), 0);
	}
	else if (flag == 2) { // White silhouette.
		al_draw_tinted_bitmap(obj.img, al_map_rgba(255, 255, 255, 50), (double)obj.x - (double)obj.w / 2, round((double)obj.y - (double)obj.h / 2), 0);
	}
	else if (flag == 3) { // Black silhouette.
		al_draw_tinted_bitmap(obj.img, al_map_rgba(0, 0, 0, obj.alpha), (double)obj.x - (double)obj.w / 2, round((double)obj.y - (double)obj.h / 2), 0);
	}
	
}

void draw_animation(ANIMATION_SHEET anim, float dx, float dy, int flag) {
	float xpos = anim.current_column * anim.cell_width;
	float ypos = anim.current_row * anim.cell_height;
	
	al_draw_bitmap_region(anim.sheet, xpos, ypos, anim.cell_width, anim.cell_height,\
	dx, dy, flag);
}

void draw_circle_hitbox(MovableObject obj, float radius) {
	if (obj.hidden)
		return;
	al_draw_filled_circle(obj.x + obj.hitbox_source_x_offset, obj.y + obj.hitbox_source_y_offset, obj.hitbox_radius, al_map_rgba(255, 255, 255, 90));
	al_draw_circle(obj.x + obj.hitbox_source_x_offset, obj.y + obj.hitbox_source_y_offset, obj.hitbox_radius, al_map_rgba(255, 0, 0, 95), 3);
}

bool circle_collision_test(MovableObject obj1, MovableObject obj2) {
	float xdist = obj1.x + obj1.hitbox_source_x_offset - obj2.x - obj2.hitbox_source_x_offset;
	float ydist = obj1.y + obj1.hitbox_source_y_offset - obj2.y - obj2.hitbox_source_y_offset;
	float dist = sqrt((double)xdist * (double)xdist + (double)ydist * (double)ydist);
	if (dist <= obj1.hitbox_radius + obj2.hitbox_radius)
		return true;
	else
		return false;
}

bool circle_line_collision_test(MovableObject obj, float x1, float y1, float x2, float y2) {
	// Method: A dot's distance between two ends of a line is equal to
	// the line's length if it's on the line.
	float dx1 = obj.x + obj.hitbox_source_x_offset - x1;
	float dy1 = obj.y + obj.hitbox_source_y_offset - y1;
	float dx2 = obj.x + obj.hitbox_source_x_offset - x2;
	float dy2 = obj.y + obj.hitbox_source_y_offset - y2;
	float dx3 = x1 - x2;
	float dy3 = y1 - y2;
	double dist1_square = (double)dx1 * dx1 + (double)dy1 * dy1; // c
	double dist2_square = (double)dx2 * dx2 + (double)dy2 * dy2; // a
	double dist3_square = (double)dx3 * dx3 + (double)dy3 * dy3; // b
	// sin = sqrt(1 - cos^2)
	
	double cosine = (dist2_square + dist3_square - dist1_square) / (2*sqrt(dist2_square * dist3_square));
	double sine_square = 1 - cosine * cosine;
	float dist_square = dist2_square*sine_square;
	
	if (dist1_square - dist_square > dist3_square) {
		dist_square = dist2_square;
	}
	else if (dist2_square - dist_square > dist3_square) {
		dist_square = dist1_square;
	}

	if (dist_square <= obj.hitbox_radius*obj.hitbox_radius) return true;
	else return false;
}


void unit_vector(float dx, float dy, float* vx, float* vy) {
	float dist = sqrt((double)dx * dx + (double)dy * dy);
	*vx = dx / dist;
	*vy = dy / dist;
}

void rotate_vector(float* vx, float* vy, double theta) {
	float sine = sin(theta * PI / 180);
	float cosine = cos(theta * PI / 180);
	float r_x = (*vx) * cosine - (*vy) * sine;
	float r_y = (*vx) * sine + (*vy) * cosine;
	*vx = r_x;
	*vy = r_y;
}

void draw_parallelogram(float x1, float y1, float x2, float y2, float d, ALLEGRO_COLOR color, int flag) {
	float x3 = x2 + d;
	float x4 = x1 + d;
	float y3 = y2;
	float y4 = y1;
	if (flag == 1) {
		al_draw_filled_triangle(x1, y1, x2, y2, x3, y3, color);
		al_draw_filled_triangle(x1, y1, x3, y3, x4, y4, color);
	}
	else if (flag == 0) {
		al_draw_triangle(x1, y1, x2, y2, x3, y3, color, 4);
		al_draw_triangle(x1, y1, x3, y3, x4, y4, color, 4);
	}
	else {
		game_abort("invalid flag passed to draw_parallelogram()");
	}
	

}

void draw_dotted_line(float x1, float y1, float x2, float y2, ALLEGRO_COLOR color) {
	float unit_vx, unit_vy;
	float dx = x2 - x1;
	float dy = y2 - y1;
	unit_vector(dx, dy, &unit_vx, &unit_vy);
	float x = x1 + unit_vx / 2;
	float y = y1 + unit_vy / 2;
	float dist = sqrt((double)dx * dx + (double)dy * dy);
	int j = 0;
	for (j = 1; j < dist; j++) {
		al_draw_filled_circle(x + unit_vx * j, y + unit_vy * j, 1, color);
	}
}

void draw_expanding_fading_circle(float x, float y, float thickness, float r, float g, float b, float* alpha, float rate, bool* fading, bool expanding) {
	if (*alpha < 0 || *alpha > 255) {
		*fading = false;
		return;
	}
	
	float radius;
	if (expanding) {
		radius = fabs(rate)*((double)255 - (double)*alpha);
	}
	else {
		radius = fabs(rate)*(*alpha);
	}
	

	if (thickness == 0) {
		al_draw_filled_circle(x, y, radius, al_map_rgba(r, g, b, *alpha));
	}
	else {
		al_draw_circle(x, y, radius, al_map_rgba(r, g, b, *alpha), thickness);
	}

	*alpha += rate;
}

void draw_player_UI_elements() {
	/* Health Bar */
	// Full Width: 452
	// Source: 70, SCREEN_H | 86, SCREEN_H-32.

	// Bar Backdrop (Dark Green)
	draw_parallelogram(70, SCREEN_H, 86, SCREEN_H-32, 452, \
		al_map_rgb(0, 30, 0), 1);
	ALLEGRO_COLOR health_bar_color;
	// Health Bar, color and length changes according to HP ratio.
	// Red <-> Yellow <-> Green
	float HP_ratio = player.current_HP / player.max_HP;

	
	static float current_HP_bar_ratio = 0;
	if (current_HP_bar_ratio > HP_ratio) {
		current_HP_bar_ratio -= 0.01;
		if (current_HP_bar_ratio < HP_ratio) current_HP_bar_ratio = HP_ratio;
	}
	else if (current_HP_bar_ratio < HP_ratio) {
		current_HP_bar_ratio += 0.01;
		if (current_HP_bar_ratio > HP_ratio) current_HP_bar_ratio = HP_ratio;
	}

	health_bar_color = al_map_rgb(255 * (1 - current_HP_bar_ratio),\
		255 * current_HP_bar_ratio, 0);
	draw_parallelogram(70, SCREEN_H, 86, SCREEN_H - 32, \
		452*current_HP_bar_ratio, health_bar_color, 1);


	

	/* Life Bar */
	// Full Width: 75
	// Source: 447, SCREEN_H | 455, SCREEN_H-15.

	// Bar Backdrop (Dark Gray)
	draw_parallelogram(447, SCREEN_H, 455, SCREEN_H - 15, 75, \
		al_map_rgb(70, 70, 70), 1);

	int i;

	// Five shades of red for lives.
	for (i = 0; i < lives; i++) {
		draw_parallelogram(447+15*i, SCREEN_H, 455+15*i, SCREEN_H - 15, 15, \
			al_map_rgb(255, 35*(i+2), 35*(i+2)), 1);
	}


	/* Sync Bar */
	// Full Width: 227
	// Source: 542, SCREEN_H-9 | 560, SCREEN_H-46.

	// Bar Backdrop (White)
	draw_parallelogram(542, SCREEN_H - 9, 560, SCREEN_H - 46, 227,\
		al_map_rgb(255, 255, 255), 1);

	// Bar color (Brighter and brighter green)
	draw_parallelogram(542, SCREEN_H - 9, 560, SCREEN_H - 46,\
		227*sync_percentage,\
		al_map_rgb(100 + 100 * sync_percentage, 255, 100 + 100 * sync_percentage), 1);

	// Additional yellow section in the center to make 100% more obvious.
	if (sync_percentage == 1) {
		draw_parallelogram(542+38, SCREEN_H - 9, 560+38, \
			SCREEN_H - 46, 227-76, al_map_rgb(255, 255, 125), 1);
	}

	/* Score Display */
	// Full Width: Varies
	// Source: 792, SCREEN_H-76. RIGHT ALIGNED. (ALLEGRO_ALIGN_RIGHT)
	// Padding: 000,000,000,...
	al_draw_textf(font_advanced_pixel_lcd_10, al_map_rgb(255, 255, 105),\
		792, SCREEN_H-76, ALLEGRO_ALIGN_RIGHT, "SCORE: %03d.%03d.%03d", score/1000000,\
		(score%1000000 - score%1000)/1000, score%1000);
	

}

void death_routine() {
	al_play_sample(death_sfx, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, &death_sfx_id);
	lives--;
	game_log("Player loses a life.");
	dying = true;
	dying_alpha = 255;

	respawn_x = player.x;
	respawn_y = player.y;
	if (lives <= 0) {
		game_log("Player has run out of lives.");
		return;
	}
	respawning = true;
	respawning_alpha = 255;

	player.current_HP = player.max_HP;

	game_log("Current HP replanished back to %d.", player.current_HP);
}

void laser_array_routine(double now) {
	if (laser_array.vertical) {
		laser_array.y += laser_array.v;
		if (laser_array.y > SCREEN_H) {
			laser_array.y = 0;
			// Randomize again after each cycle.
			laser_array.vert_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
			if(laser_array.mirror) laser_array.vert_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
			if (laser_array.v < laser_array.v_limit)
				laser_array.v += laser_array.a;
			else laser_array.limit_reached = true;
		}
		if ((now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
			if (pnt_in_rect(player.x, player.y, -player.hitbox_radius, \
				laser_array.y - SCREEN_H / 16 - \
				player.hitbox_radius, \
				laser_array.vert_gap_pos - laser_array.gap_halfwidth + \
				player.hitbox_radius * 2, SCREEN_H / 16 + \
				player.hitbox_radius * 2) || \
				pnt_in_rect(player.x, player.y, laser_array.vert_gap_pos + \
					laser_array.gap_halfwidth - player.hitbox_radius, \
					laser_array.y - SCREEN_H / 16 - \
					player.hitbox_radius, \
					SCREEN_W + player.hitbox_radius * 2, \
					SCREEN_H / 16 + player.hitbox_radius * 2)) {
				al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= 1.25 * boss.base_damage;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}
		}

		if (laser_array.mirror) {
			float mirror_y = SCREEN_H - laser_array.y;
			if ((now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
				if (pnt_in_rect(player.x, player.y, -player.hitbox_radius, \
					mirror_y - SCREEN_H / 16 - \
					player.hitbox_radius, \
					laser_array.vert_gap_pos_mirror - laser_array.gap_halfwidth + \
					player.hitbox_radius * 2, SCREEN_H / 16 + \
					player.hitbox_radius * 2) || \
					pnt_in_rect(player.x, player.y, laser_array.vert_gap_pos_mirror + \
						laser_array.gap_halfwidth - player.hitbox_radius, \
						mirror_y - SCREEN_H / 16 - \
						player.hitbox_radius, \
						SCREEN_W + player.hitbox_radius * 2, \
						SCREEN_H / 16 + player.hitbox_radius * 2)) {
					al_play_sample(player_damaged_sfx, 1.2, 0.2, 1, ALLEGRO_PLAYMODE_ONCE, &player_damaged_sfx_id);
					game_log("Player takes damage.");
					last_damage_timestamp = now;
					player.current_HP -= 1.25 * boss.base_damage;
					combo = 1; // Reset combo.
					if (player.current_HP <= 0) {
						death_routine();
					}
				}
			}
		}
	}

	if (laser_array.horizontal) {
		laser_array.x += laser_array.v;
		if (laser_array.x > SCREEN_W) {
			laser_array.x = 0;
			// Randomize again after each cycle.
			laser_array.hori_gap_pos = laser_array.gap_halfwidth + rand() % (SCREEN_H - (int)laser_array.gap_halfwidth);
			if (laser_array.mirror) laser_array.hori_gap_pos_mirror = laser_array.gap_halfwidth + rand() % (SCREEN_W - (int)laser_array.gap_halfwidth);
			if (laser_array.v < laser_array.v_limit)
				laser_array.v += laser_array.a;
			else laser_array.limit_reached = true;
		}
		if ((now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active) {
			if (pnt_in_rect(player.x, player.y, laser_array.x - SCREEN_W / 16 - \
				player.hitbox_radius, -player.hitbox_radius, \
				SCREEN_W / 16 + \
				player.hitbox_radius * 2, \
				laser_array.hori_gap_pos - laser_array.gap_halfwidth + \
				player.hitbox_radius * 2) || \
				pnt_in_rect(player.x, player.y, laser_array.x - SCREEN_W / 16 - \
					player.hitbox_radius, laser_array.hori_gap_pos + \
					laser_array.gap_halfwidth + player.hitbox_radius, \
					SCREEN_W / 16 + player.hitbox_radius * 2, \
					SCREEN_H + player.hitbox_radius * 2)) {
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= 1.25 * boss.base_damage;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}
		}

		if (laser_array.mirror) {
			float mirror_x = SCREEN_W - laser_array.x;
			if ((now - last_damage_timestamp) / 100000 >= MAX_DAMAGE_COOLDOWN && !ult_active)
			if (pnt_in_rect(player.x, player.y, mirror_x - SCREEN_W / 16 - \
				player.hitbox_radius, -player.hitbox_radius, \
				SCREEN_W / 16 + \
				player.hitbox_radius * 2, \
				laser_array.hori_gap_pos_mirror - laser_array.gap_halfwidth + \
				player.hitbox_radius * 2) || \
				pnt_in_rect(player.x, player.y, mirror_x - SCREEN_W / 16 - \
					player.hitbox_radius, laser_array.hori_gap_pos_mirror + \
					laser_array.gap_halfwidth + player.hitbox_radius, \
					SCREEN_W / 16 + player.hitbox_radius * 2, \
					SCREEN_H + player.hitbox_radius * 2)) {
				game_log("Player takes damage.");
				last_damage_timestamp = now;
				player.current_HP -= 1.25 * boss.base_damage;
				combo = 1; // Reset combo.
				if (player.current_HP <= 0) {
					death_routine();
				}
			}
		}
	}
}

ALLEGRO_BITMAP *load_bitmap_resized(const char *filename, int w, int h) {
	ALLEGRO_BITMAP* loaded_bmp = al_load_bitmap(filename);
	if (!loaded_bmp)
		game_abort("failed to load image: %s", filename);
	ALLEGRO_BITMAP *resized_bmp = al_create_bitmap(w, h);
	ALLEGRO_BITMAP *prev_target = al_get_target_bitmap();

	if (!resized_bmp)
		game_abort("failed to create bitmap when creating resized image: %s", filename);
	al_set_target_bitmap(resized_bmp);
	al_draw_scaled_bitmap(loaded_bmp, 0, 0,
		al_get_bitmap_width(loaded_bmp),
		al_get_bitmap_height(loaded_bmp),
		0, 0, w, h, 0);
	al_set_target_bitmap(prev_target);
	al_destroy_bitmap(loaded_bmp);

	game_log("resized image: %s", filename);

	return resized_bmp;
}

bool pnt_in_rect(int px, int py, int x, int y, int w, int h) {
	if (px > x&& px < x + w && py > y&& py < y + h)
		return true;
	else
		return false;
}


// +=================================================================+
// | Code below is for debugging purpose, it's fine to remove it.    |
// | Deleting the code below and removing all calls to the functions |
// | doesn't affect the game.                                        |
// +=================================================================+

void game_abort(const char* format, ...) {
	va_list arg;
	va_start(arg, format);
	game_vlog(format, arg);
	va_end(arg);
	fprintf(stderr, "error occured, exiting after 2 secs");
	// Wait 2 secs before exiting.
	al_rest(2);
	// Force exit program.
	exit(1);
}

void game_log(const char* format, ...) {
#ifdef LOG_ENABLED
	va_list arg;
	va_start(arg, format);
	game_vlog(format, arg);
	va_end(arg);
#endif
}

void game_vlog(const char* format, va_list arg) {
#ifdef LOG_ENABLED
	static bool clear_file = true;
	vprintf(format, arg);
	printf("\n");
	// Write log to file for later debugging.
	FILE* pFile = fopen("log.txt", clear_file ? "w" : "a");
	if (pFile) {
		vfprintf(pFile, format, arg);
		fprintf(pFile, "\n");
		fclose(pFile);
	}
	clear_file = false;
#endif
}
