#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xcomposite.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include "config.h"
#include "struct.h"

#define WORKSPACE 4

int8_t current_workspace = 0;

const char * make_window_floating[] = {
  "Thunar",
  "flameshot",
  "Alacritty",
};


const char *startup_commands[] = {
  "killall dunst &",
  "setxkbmap us &",
  "nitrogen --restore &",
  "dunst --config ~/.config/i3/dunstrc &",
  "picom &" ,
  "polybar &"
};


static bool wm_detected = false;
static bd26 wm;

static void handle_create_notify(XCreateWindowEvent e) { (void)e;}
static void handle_configure_notify(XConfigureEvent e) {(void)e;}
static int handle_wm_detected(Display *display, XErrorEvent *e){(void)display; wm_detected = ((int32_t)e->error_code == BadAccess); return 0;}
static void handle_reparent_notify(XReparentEvent e){(void)e;}
static void handle_destroy_notify(XDestroyWindowEvent e){(void)e;}
static void handle_map_notify(XMapEvent e) {(void)e;}
static void handle_button_release(XButtonEvent e) {(void)e;}
static void handle_key_release(XKeyEvent e) {(void)e;}
static int handle_x_error(Display *display, XErrorEvent *e) {    (void)display;
    char err_msg[1024];
    XGetErrorText(display, e->error_code, err_msg, sizeof(err_msg));
    printf("X Error:\n\tRequest: %i\n\tError Code: %i - %s\n\tResource ID: %i\n", 
           e->request_code, e->error_code, err_msg, (int)e->resourceid); return 0;
}
static void handle_configure_request(XConfigureRequestEvent e);
static void handle_map_request(XMapRequestEvent e); //okay
static void handle_unmap_notify(XUnmapEvent e);
static void handle_key_press(XKeyEvent e);
static void handle_button_press(XButtonEvent e);
static void handle_motion_notify(XMotionEvent e);
static void handle_poperty_notify(XEvent *e);
//-----Other Functions start
static void set_fullscreen(Window win);
static void unset_fullscreen(Window win);
static void window_frame(Window win);
static void window_unframe(Window win);
static Window get_frame_window(Window win);
static int32_t get_client_index(Window win);
static void move_client(Client *client, Vec2 pos);
static void resize_client(Client *client, Vec2 sz);
static void grab_global_key();
static void grab_window_key(Window win);
static void establish_window_layout(bool restore_back);
static void change_workspace();
static void run_bd26();
static void change_focus_window(Window win);
static void mini_app();
static void change_active_workspace();
static void print_workspace_number();
//------other Functions end

//tiling related function


void change_active_window(){
  int active_workspace = current_workspace + 1;

  for (int i = 0; i < WORKSPACE; i++){
    active_workspace %= 4;
    if (wm.clients_count[active_workspace] != 0){
      active_workspace = active_workspace;
      break;
    }
    active_workspace++;
  }

  if (active_workspace == current_workspace) return;
  
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XUnmapWindow(wm.display, wm.client_windows[current_workspace][i].frame);
  }
    for (uint32_t i = 0; i < wm.clients_count[active_workspace]; i++){
    XMapWindow(wm.display, wm.client_windows[active_workspace][i].frame);
      if (wm.client_windows[active_workspace][i].was_focused)
        XSetInputFocus(wm.display, wm.client_windows[active_workspace][i].win, RevertToPointerRoot, CurrentTime);
  }
  current_workspace = active_workspace;
  print_workspace_number();
}



void mini_app(){
  if (wm.clients_count[current_workspace] < 3){
    wm.currentstate[current_workspace] = NORMAL_STATE;
    return;
  }
  wm.currentstate[current_workspace] = MINI_STATE;
  int width = DISPLAY_WIDTH / 3;
  int row_counts = (wm.clients_count[current_workspace] % 3) ? 
  (wm.clients_count[current_workspace] / 3 + 1) : wm.clients_count[current_workspace] / 3;
  int height = DISPLAY_HEIGHT / row_counts;
  int x = 10, y = 10;
  int col_count = 3;
  uint32_t counter = 0;
  for (int i = 0; i < row_counts; i++){
    for (int j = 0; j < col_count; j++){
      if (counter == 0 || counter == 1 || counter == 2) y = 28;
      move_client(&wm.client_windows[current_workspace][counter], (Vec2){.x = x, .y = y});
      resize_client(&wm.client_windows[current_workspace][counter], (Vec2){.x = width - 18, .y = height - 28});
      x += width + 10;
      counter++;
      if (counter % 3 == 0){
        x = 10;
        y += height;
      }
    }
    if (counter > wm.clients_count[current_workspace]) break;
  }
}




void handle_poperty_notify(XEvent * event){
    Atom atom = XInternAtom(wm.display, "WM_CLASS", False);
    if (event->xproperty.atom == atom) {
        XTextProperty prop;
        char **class_name;
        int count;

        XGetTextProperty(wm.display, event->xproperty.window, &prop, atom);
        if (prop.nitems > 0 && prop.value) {
            if (XmbTextPropertyToTextList(wm.display, &prop, &class_name, &count) == Success && count > 0) {
                printf("Class name: %s\n", class_name[0]);
                XFreeStringList(class_name);
            }
        }
        XFree(prop.value);
    }
}

void change_focus_window(Window win){
  uint32_t client_index = get_client_index(win);

  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XSetWindowBorder(wm.display, wm.client_windows[current_workspace][i].frame, UBORDER_COLOR);
    wm.client_windows[current_workspace][i].was_focused = false;
  }
  XSetWindowBorder(wm.display, wm.client_windows[current_workspace][client_index].frame, FBORDER_COLOR);
  XRaiseWindow(wm.display, wm.client_windows[current_workspace][client_index].frame);
  XSetInputFocus(wm.display, wm.client_windows[current_workspace][client_index].win, RevertToPointerRoot, CurrentTime);
  wm.client_windows[current_workspace][client_index].was_focused = true;
}

void print_workspace_number(){
  char command[50];
  sprintf(command, "echo %d > ~/test.txt", current_workspace);
  system(command);
}

void swap(Client *client1, Client *client2){
  Client tmp = *client1;
  *client1 = *client2;
  *client2 = tmp;
  establish_window_layout(false);
}


void change_workspace(){
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XUnmapWindow(wm.display, wm.client_windows[current_workspace][i].frame);
  }
  current_workspace++;
  if (current_workspace >= WORKSPACE) current_workspace -= WORKSPACE;
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
      XMapWindow(wm.display, wm.client_windows[current_workspace][i].frame);
      if (wm.client_windows[current_workspace][i].was_focused)
        XSetInputFocus(wm.display, wm.client_windows[current_workspace][i].win, RevertToPointerRoot, CurrentTime);
  }
  print_workspace_number();
}

void change_workspace_back(){
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XUnmapWindow(wm.display, wm.client_windows[current_workspace][i].frame);
  }
  current_workspace--;
  if (current_workspace < 0){
    current_workspace += WORKSPACE;
  }
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XMapWindow(wm.display, wm.client_windows[current_workspace][i].frame);
      if (wm.client_windows[current_workspace][i].was_focused)
        XSetInputFocus(wm.display, wm.client_windows[current_workspace][i].win, RevertToPointerRoot, CurrentTime);
  }
  print_workspace_number();
}



void establish_window_layout(bool restore_back){
  Client * tmp_clients[CLIENT_WINDOW_CAP];
  uint32_t clients_count = 0;

  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    if (!wm.client_windows[current_workspace][i].is_floating || restore_back){
      tmp_clients[clients_count++] = &wm.client_windows[current_workspace][i];
    }
  }
  if (restore_back){
  for (uint32_t i = 0; i < clients_count; i++){
    tmp_clients[i]->is_floating = false;
  }
  }

  if (clients_count == 0)return;
  if (wm.current_layout[current_workspace] == WINDOW_LAYOUT_TILED){
    Client * rooT = tmp_clients[0];

    if (clients_count == 1){
      set_fullscreen(rooT -> frame);
      return;
    }

    resize_client(rooT, (Vec2){.x = (float)DISPLAY_WIDTH / 2, .y = DISPLAY_HEIGHT - 20});
    move_client(rooT, (Vec2){.x = 5, .y = 30});
    rooT -> fullscreen = false;
    float y_cordintae = 30;

    for (uint32_t i = 1; i < clients_count; i++){
      resize_client(tmp_clients[i], (Vec2){.x = ((float)DISPLAY_WIDTH / 2) - 22, .y = ((float)DISPLAY_HEIGHT / (clients_count - 1) - 20)});
      move_client(tmp_clients[i], (Vec2){.x = ((float)DISPLAY_WIDTH / 2 + 15), .y = y_cordintae});
      y_cordintae += ((float) DISPLAY_HEIGHT / (clients_count - 1));
    }
  }
}




void establish_window_layout_bak(){
  int32_t master_index = -1;
  uint32_t clients_on_monitor = 0;
  bool found_master = false;

  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
      if (!found_master){
        master_index = i;
        found_master = true;
      }
      clients_on_monitor++;
  }

  if (clients_on_monitor == 0 || !found_master) return;

  Client *master = &wm.client_windows[current_workspace][master_index];

  if (wm.current_layout[current_workspace] == WINDOW_LAYOUT_TILED){
    if (clients_on_monitor == 1){
      set_fullscreen(master->frame);
      return;
    }
    int fixed = (DISPLAY_WIDTH / clients_on_monitor) - wm.window_gap;
    int x = wm.window_gap;
    for (uint32_t i = 0; i < clients_on_monitor; i++){
      resize_client(&wm.client_windows[current_workspace][i], (Vec2) {.x = fixed - wm.window_gap, .y = DISPLAY_HEIGHT - 30});
      move_client(&wm.client_windows[current_workspace][i], (Vec2) {.x = x, .y = 15});
      x += fixed + wm.window_gap;
    }
  }
}

void resize_client(Client *client , Vec2 sz) {
  XWindowAttributes attributes;
  XGetWindowAttributes(wm.display, client -> win, &attributes);

  if (sz.x >= DISPLAY_WIDTH && sz.y >= DISPLAY_HEIGHT){
    client -> fullscreen = true;
    XSetWindowBorderWidth(wm.display, client->frame, 0);
    client -> fullscreen_revert_size = (Vec2) {.x = attributes.width, .y = attributes.height};
    // client -> fullscreen_revert_pos = (Vec2) {.x = attributes.x, .y = attributes.y};
  }
  else {
    client -> fullscreen = false;
    XSetWindowBorderWidth(wm.display, client->frame, BORDER_WIDTH);
  }

  XResizeWindow(wm.display, client -> win, sz.x, sz.y);
  XResizeWindow(wm.display, client -> frame, sz.x, sz.y);
  XRaiseWindow(wm.display, client -> frame);
}

void move_client(Client *client, Vec2 pos){
  XMoveWindow(wm.display, client -> frame, pos.x, pos.y);
}



void set_fullscreen(Window win){
  if (win == wm.root) return;

  uint32_t client_index = get_client_index(win);
  if (wm.client_windows[current_workspace][client_index].fullscreen) return;

  XWindowAttributes attribs;
  XGetWindowAttributes(wm.display, wm.client_windows[current_workspace][client_index].frame, &attribs);

  wm.client_windows[current_workspace][client_index].fullscreen_revert_pos = (Vec2) {.x = attribs.x, .y = attribs.y};
  wm.client_windows[current_workspace][client_index].fullscreen_revert_size = (Vec2) {.x = attribs.width, .y = attribs.height};

  resize_client(&wm.client_windows[current_workspace][client_index], (Vec2) {.x = (float) DISPLAY_WIDTH - 20, .y =  DISPLAY_HEIGHT - 20} );

  move_client(&wm.client_windows[current_workspace][client_index], (Vec2){.x = 9.7, .y = 32.5});
  wm.client_windows[current_workspace][client_index].fullscreen = true;
}

void unset_fullscreen(Window win){
  if (win == wm.root) return;
  const uint32_t client_index = get_client_index(win);

  resize_client(&wm.client_windows[current_workspace][client_index], wm.client_windows[current_workspace][client_index].fullscreen_revert_size);
  move_client(&wm.client_windows[current_workspace][client_index], wm.client_windows[current_workspace][client_index].fullscreen_revert_pos);
  wm.client_windows[current_workspace][client_index].fullscreen = false;
}


//others but dorakri start
void window_frame(Window win){
  if (get_client_index(win) != -1) return;
  XWindowAttributes attribs;
  XGetWindowAttributes(wm.display, win, &attribs);

  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    XSetWindowBorder(wm.display, wm.client_windows[current_workspace][i].frame, UBORDER_COLOR);
  }

  const Window win_frame = XCreateSimpleWindow(
    wm.display,
    wm.root,
    attribs.x,
    attribs.y,
    attribs.width,
    attribs.height,
    BORDER_WIDTH,
    FBORDER_COLOR,
    BG_COLOR
    );
  XCompositeRedirectWindow(wm.display, win_frame, CompositeRedirectAutomatic);
  //Select Input for the win_frame
  XSelectInput(wm.display, win_frame, SubstructureNotifyMask | SubstructureRedirectMask);
  XAddToSaveSet(wm.display, win_frame);
  XReparentWindow(wm.display, win, win_frame, 0, 0);
  XMapWindow(wm.display, win_frame);
  XSetInputFocus(wm.display, win, RevertToPointerRoot, CurrentTime);

  wm.client_windows[current_workspace][wm.clients_count[current_workspace]++] = (Client) {.win = win, .frame = win_frame, .fullscreen = attribs.width >= DISPLAY_WIDTH && attribs.height >= DISPLAY_HEIGHT};
  grab_window_key(win);
  wm.client_windows[current_workspace][wm.clients_count[current_workspace]].is_floating = false;

  XClassHint classhint;
  if (XGetClassHint(wm.display, win, &classhint)){
    for (int i = 0; i < sizeof(make_window_floating) / sizeof(make_window_floating[0]); i++){
      if (strcmp(classhint.res_class, make_window_floating[i]) == 0){
        wm.client_windows[current_workspace][get_client_index(win_frame)].is_floating = true;
        change_focus_window(win);
      }
    }
  }
  XFree(classhint.res_class);

  if (wm.currentstate[current_workspace] == MINI_STATE) mini_app();
  else
    establish_window_layout(false);
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    wm.client_windows[current_workspace][i].was_focused = false;
  }
  wm.client_windows[current_workspace][get_client_index(win_frame)].was_focused = true;


  Window win_decoration = XCreateSimpleWindow(wm.display, wm.root, 0, 0, attribs.width, attribs.height, BORDER_WIDTH, FBORDER_COLOR, BG_COLOR);
  XUnmapWindow(wm.display, win_decoration);
  XWindowAttributes attribs_frame;
  XGetWindowAttributes(wm.display, win_frame, &attribs_frame);

  wm.client_windows[current_workspace][wm.clients_count[current_workspace] - 1].decoration.close_icon = 
    XCreateSimpleWindow(wm.display, win_decoration, attribs_frame.width - 15, 0, 15, 20, 0, FBORDER_COLOR, 0xff0000);
  XSelectInput(wm.display, wm.client_windows[current_workspace][wm.clients_count[current_workspace] - 1].decoration.close_icon, ButtonPressMask | SubstructureNotifyMask | SubstructureRedirectMask);
  XMapWindow(wm.display, wm.client_windows[current_workspace][wm.clients_count[current_workspace]- 1].decoration.close_icon);
  XReparentWindow(wm.display, wm.client_windows[current_workspace][wm.clients_count[current_workspace] - 1].decoration.close_icon, win_frame, attribs_frame.width - 15, 0);
}

void window_unframe(Window win){
  int32_t client_index = get_client_index(win);

  if (client_index == -1) {printf("Returning from unframe");return;}
  const Window frame_window = wm.client_windows[current_workspace][client_index].frame;

  XReparentWindow(wm.display, frame_window, wm.root, 0, 0);
  XReparentWindow(wm.display, win, wm.root, 0, 0);
  XUnmapWindow(wm.display, frame_window);

  for (uint32_t i = client_index; i < wm.clients_count[current_workspace] - 1; i++)
  {
    printf("SWAPPING\n\n\n\n");
    printf("CLIENT INDEX IS %d : %d\n\n\n\n", wm.clients_count[current_workspace] ,i);
    wm.client_windows[current_workspace][i] = wm.client_windows[current_workspace][i + 1];
  }
  wm.clients_count[current_workspace]--;
  printf("CLIENT INDEX IS %d\n\n\n\n", wm.clients_count[current_workspace]);
  if (wm.clients_count[current_workspace] != 0){
    if (client_index == 0) client_index = 1;
    change_focus_window(wm.client_windows[current_workspace][client_index - 1].win);
  }

  if (wm.currentstate[current_workspace] == NORMAL_STATE || wm.clients_count[current_workspace] < 3){
    establish_window_layout(false);
    wm.currentstate[current_workspace] = NORMAL_STATE;
  }
  else
    mini_app();
}
//others but dorakri end


//choto khato functions start

int32_t get_client_index(Window win){
  for (uint32_t i  = 0; i < wm.clients_count[current_workspace]; i++)
    if (wm.client_windows[current_workspace][i].win == win || wm.client_windows[current_workspace][i].frame == win)
      return i;
  return -1;
}

Window get_frame_window(Window win){
  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++)
    if (wm.client_windows[current_workspace][i].win  == win || wm.client_windows[current_workspace][i].frame == win)
      return wm.client_windows[current_workspace][i].frame;
  return 0;
}

//choto khato functions end

//DORKARI FUNCTIONS
void handle_map_request(XMapRequestEvent e){
  window_frame(e.window);
  XMapWindow(wm.display, e.window);
  XSetInputFocus(wm.display, e.window, RevertToPointerRoot, CurrentTime);
}

void handle_unmap_notify(XUnmapEvent e){
  if (get_client_index(e.window) == -1) {
    printf("Ignore UnmapNotify for non-client window\n");
    return;
  }
  window_unframe(e.window);
}

void handle_configure_request(XConfigureRequestEvent e){
  {
  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.height = e.height;
  changes.width = e.width;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail; XConfigureWindow(wm.display, e.window, e.value_mask, &changes);
  }
  {
  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.height = e.height;
  changes.width = e.width;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;
  XConfigureWindow(wm.display, get_frame_window(e.window), e.value_mask, &changes);
  }
}

void handle_button_press(XButtonEvent e){
  Window frame = get_frame_window(e.window);
  wm.cursor_start_pos = (Vec2) {.x = (float)e.x_root, .y = (float)e.y_root};
  Window root;
  int32_t x, y;
  unsigned width, height, border_width, depth;

  XGetGeometry(wm.display, frame, &root, &x, &y, &width, &height, &border_width, &depth);
  wm.cursor_start_frame_pos = (Vec2){.x = (float)x, .y = (float)y};
  wm.cursor_start_frame_size = (Vec2){.x = (float)width, .y = (float)height};

  XRaiseWindow(wm.display, wm.client_windows[current_workspace][get_client_index(e.window)].win);
  XSetInputFocus(wm.display, e.window, RevertToPointerRoot, CurrentTime);

  if (e.button == Button1 && wm.currentstate[current_workspace] == MINI_STATE && e.window != wm.root){
    if (e.state & ShiftMask){
      set_fullscreen(e.window);
      change_focus_window(e.window);
      wm.currentstate[current_workspace] = NORMAL_STATE;
    }
  }


  for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
    if (wm.client_windows[current_workspace][i].decoration.close_icon == e.window){
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.message_type =
            XInternAtom(wm.display, "WM_PROTOCOLS", false);
        msg.xclient.window = e.window;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] =
            XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
        XSendEvent(wm.display, e.window, false, 0, &msg);
        break;
    }
  }

}

void handle_motion_notify(XMotionEvent e) {
  // Window frame = get_frame_window(e.window);
  Vec2 drag_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};
  Vec2 delta_drag = (Vec2){.x = drag_pos.x - wm.cursor_start_pos.x,
                           .y = drag_pos.y - wm.cursor_start_pos.y};
  Client * tmp_client = &wm.client_windows[current_workspace][get_client_index(e.window)];

  if (e.state & Button1Mask) {
    /* Pressed MOD + left mouse */

    Vec2 drag_dest =
        (Vec2){.x = (float)(wm.cursor_start_frame_pos.x + delta_drag.x),
               .y = (float)(wm.cursor_start_frame_pos.y + delta_drag.y)};
      if (wm.client_windows[current_workspace][get_client_index(e.window)].fullscreen) {
        tmp_client -> fullscreen = false;
      }
      change_focus_window(e.window);
      move_client(tmp_client, drag_dest);
      if (!tmp_client -> is_floating){
        tmp_client -> is_floating = true;
        if (wm.currentstate[current_workspace] == MINI_STATE) mini_app();
        else establish_window_layout(false);
      }
  } else if (e.state & Button3Mask) {
    /* Pressed MOD + right mouse*/
    if (wm.client_windows[current_workspace][get_client_index(e.window)].fullscreen) return;

    Vec2 resize_delta =
        (Vec2){.x = MAX(delta_drag.x, -wm.cursor_start_frame_size.x),
               .y = MAX(delta_drag.y, -wm.cursor_start_frame_size.y)};
    Vec2 resize_dest =
        (Vec2){.x = wm.cursor_start_frame_size.x + resize_delta.x,
               .y = wm.cursor_start_frame_size.y + resize_delta.y};
    
    for (uint32_t i = 0; i < wm.clients_count[current_workspace]; i++){
      if (wm.client_windows[current_workspace][i].frame == tmp_client -> frame){
        XSetWindowBorder(wm.display, tmp_client->frame, FBORDER_COLOR);
        continue;
      }
      XSetWindowBorder(wm.display , wm.client_windows[current_workspace][i].frame, UBORDER_COLOR);
    }
    resize_client(tmp_client, resize_dest);
    if (!tmp_client -> is_floating){
      tmp_client -> is_floating = true;
      establish_window_layout(false);
    }
  }
}


void grab_global_key(){
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, OPEN_TERMINAL), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, OPEN_BROWSER), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, OPEN_LAUNCHER), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, KILL_WM), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, MAKE_TILE), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CHANGE_WORKSPACE), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, VOLUME_UP), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, VOLUME_DOWN), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, VOLUME_MUTE), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CHANGE_WORKSPACE_BACK), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, MINI_APP), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CHANGE_ACTIVE_WORKSPACE), MOD, wm.root, false, GrabModeAsync, GrabModeAsync);
}


void grab_window_key(Window win){
  XGrabButton(wm.display, Button1, MOD, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button3, MOD, win, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.display, Button1, ShiftMask, win, false, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, CLOSE_WINDOW), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, FULL_SCREEN), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SWAP_WINDOW), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, SWAP_UP_DOWN), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, NAVIGATE_UP), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, NAVIGATE_DOWN), MOD, win, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(wm.display, XKeysymToKeycode(wm.display, MAX_MINI_APP), MOD, win, false, GrabModeAsync, GrabModeAsync);

}

void handle_key_press(XKeyEvent e){
  //if user press MOD Key and Q then it will close the Programm
  if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, CLOSE_WINDOW)) {
      XEvent msg;
      memset(&msg, 0, sizeof(msg));
      msg.xclient.type = ClientMessage;
      msg.xclient.message_type = XInternAtom(wm.display, "WM_PROTOCOLS", false);
      msg.xclient.window = e.window;
      msg.xclient.format = 32;
      msg.xclient.data.l[0] = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
      XSendEvent(wm.display, e.window, false, 0, &msg);
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, KILL_WM)) wm.running = false;
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, OPEN_TERMINAL)) system(CMD_TERMINAL);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, OPEN_BROWSER)) system(CMD_BROWSER);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, OPEN_LAUNCHER)) system(CMD_APPLAUNCHER);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, FULL_SCREEN)) {
    if (wm.client_windows[current_workspace][get_client_index(e.window)].fullscreen) {
      unset_fullscreen(e.window);
    }
    else 
      set_fullscreen(e.window);
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, VOLUME_UP)) system(CMD_VOLUME_UP);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, VOLUME_DOWN)) system(CMD_VOLUME_DOWN);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, VOLUME_MUTE)) system(CMD_VOLUME_MUTE);
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, MAKE_TILE)) {establish_window_layout(true);
    wm.currentstate[current_workspace] = NORMAL_STATE;
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, CHANGE_WORKSPACE)) change_workspace();
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, CHANGE_WORKSPACE_BACK)) change_workspace_back();
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, SWAP_WINDOW)){
    if (wm.clients_count[current_workspace] >= 2){
      uint32_t client_index = get_client_index(e.window);
      if (client_index == 0){
        swap(&wm.client_windows[current_workspace][0], 
          &wm.client_windows[current_workspace][1]
          );
      }

      else{
        swap(
          &wm.client_windows[current_workspace][0],
          &wm.client_windows[current_workspace][client_index]
        );
      }
    }
  }
  else if (e.state & (MOD | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, SWAP_UP_DOWN)){
    if (wm.clients_count[current_workspace] >= 3){
      uint32_t tmp_index = get_client_index(e.window);
      if (tmp_index == 1) tmp_index = 2;
      swap(&wm.client_windows[current_workspace][tmp_index], &wm.client_windows[current_workspace][tmp_index - 1]);
    }
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, NAVIGATE_DOWN)){
    if (wm.clients_count[current_workspace] > 1){
      uint32_t client_index = get_client_index(e.window);
      if (client_index == wm.clients_count[current_workspace] - 1) client_index = -1;
      change_focus_window(wm.client_windows[current_workspace][client_index + 1].win);
    }
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, NAVIGATE_UP)){
    if (wm.clients_count[current_workspace] > 1){
      uint32_t client_index = get_client_index(e.window);
      if (client_index == 0) client_index = wm.clients_count[current_workspace];
      change_focus_window(wm.client_windows[current_workspace][client_index -1].win);
      }
  }
  else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, MINI_APP)){
    if (wm.currentstate[current_workspace] == MINI_STATE) {
      wm.currentstate[current_workspace] = NORMAL_STATE;
      establish_window_layout(false);
    }
    else
      mini_app();
  }else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, MAX_MINI_APP)){
    wm.currentstate[current_workspace] = NORMAL_STATE;
    set_fullscreen(e.window);
    change_focus_window(e.window);
  }else if (e.state & MOD && e.keycode == XKeysymToKeycode(wm.display, CHANGE_ACTIVE_WORKSPACE)){
    change_active_window();
  }
}


void run_bd26(){

  XSetErrorHandler(handle_wm_detected);
  if (!wm.clients_count[current_workspace]) 
    wm.clients_count[current_workspace] = 0;
  wm.cursor_start_frame_size = (Vec2){.x = 0.0f, .y = 0.0f};
  wm.cursor_start_frame_pos = (Vec2){.x = 0.0f, .y = 0.0f};
  wm.cursor_start_pos = (Vec2){.x = 0.0f, .y = 0.0f};
  wm.current_layout[current_workspace] = WINDOW_LAYOUT_TILED;
  wm.window_gap = 10;
  wm.running = true;
  for (int i = 0; i < WORKSPACE; i++){
    printf("Makint the state to normal");
    wm.currentstate[i] = NORMAL_STATE;
  }
  XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask);
  XSync(wm.display, false);

  if (wm_detected){
    printf("Another Window Manager is Running\n");
    return;
  }
  XSetErrorHandler(handle_x_error);

  Cursor cursor = XcursorLibraryLoadCursor(wm.display, "arrow");
  XDefineCursor(wm.display, wm.root, cursor);
  XSetErrorHandler(handle_x_error);

  grab_global_key(); //Dispatch Event Start
  while (wm.running) {
    XEvent e;
    XNextEvent(wm.display, &e);
    switch (e.type) {
      //Minor cases
      case CreateNotify:
        handle_create_notify(e.xcreatewindow);break;
      case DestroyNotify:
        handle_destroy_notify(e.xdestroywindow);break;
      case ReparentNotify:
        handle_reparent_notify(e.xreparent);break;
      case ButtonRelease:
        handle_button_release(e.xbutton);break;
      case KeyRelease:
        handle_key_release(e.xkey);break;
      case MapNotify:
        handle_map_notify(e.xmap);break;
      case ConfigureNotify:
        handle_configure_notify(e.xconfigure);break;
      //minor cases end
      //

      case PropertyNotify:
        handle_poperty_notify(&e);break;
      case ConfigureRequest:
        handle_configure_request(e.xconfigurerequest);break;
      case MapRequest:
        handle_map_request(e.xmaprequest);break;
      case UnmapNotify:
        handle_unmap_notify(e.xunmap);break;
      case KeyPress:
        handle_key_press(e.xkey);break;
      case ButtonPress:
        handle_button_press(e.xbutton);break;
      case MotionNotify:
        while (XCheckTypedWindowEvent(wm.display, e.xmotion.window, MotionNotify, &e)) {}
        handle_motion_notify(e.xmotion);break;
    }
    }
}

bd26 init_bd26(){
  bd26 tmp;

  tmp.display = XOpenDisplay(NULL);
  if (!tmp.display){
    err(1, "Can not create the connection");
  }
  tmp.root = DefaultRootWindow(tmp.display);
  print_workspace_number();
  return tmp;
}

void close_bd26(){
  XCloseDisplay(wm.display);
}

static void run_startup_cmds(){
  uint32_t t = sizeof(startup_commands) / sizeof(startup_commands[0]);
  for (uint32_t i = 0; i < t; i++)
    system(startup_commands[i]);
}

int main(){
  wm = init_bd26();
  for (uint32_t i = 0; i < WORKSPACE; i++){
    wm.clients_count[i] = 0;
  }
  run_startup_cmds();
  run_bd26();
  close_bd26();
}

