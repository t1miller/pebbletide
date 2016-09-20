/** TODO parsing needs to handle negative tide feet case
 */

#include <pebble.h>

#define HOURS_IN_DAY 24

//Local Storage
typedef struct {
  int date;//between 1 - 365, we dont care what date is, we just want it to be unique and comparable
  int tideData[24];
} TideStruct;
static const uint32_t MESSAGE_KEY_TIDE_LOCAL_STORAGE = 1009;

//Function declerations
static void refreshGraphics();
static void inbox_received_handler(DictionaryIterator *iter, void *context);
static void outbox_sent_callback(DictionaryIterator *iter, void *context);
static void clearGraphicData();
static int getCurrentDate();
static bool saveTideDataToStorage(TideStruct tData); 
static bool loadTideDataFromStorage(TideStruct * tData);

//Constants
static const int WIDTH_OF_GUI = 140;
static const int HEIGHT_OF_GUI = 135;
static const int ZERO_LINE_GUI = 106;
static const int PX_PER_FOOT = 12;
static const float PX_THICKNESS_OF_RED_LINE = 4;
static const float PX_PER_HOUR = 6;
static const int PX_OF_TIME_LINE = 1;
static const int TIME_STARTING_OFFSET = 6;
static const int MAX_FEET = 8;
static const bool DEBUG = true;
static const int MAX_LENGTH_JS_BUFFER_OUT = 400;
static const int MAX_LENGTH_JS_BUFFER_IN = 400;

//Graphics
static Window *window;
static Layer *graphic_layer;
static TextLayer *clock_time;
static GPath *tide_path = NULL;//for filling in tide line
static GColor tideBackgroundColor;
static GPoint extremaPoints[HOURS_IN_DAY];

//data
static float tideFtData[24] = {-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1};

//This receives the message passed by the javascript portion
static void inbox_received_handler(DictionaryIterator *iter, void *context) {

  static const int MAX_DATA_PACKET_LENGTH = 2;
  Tuple *tideDataTuple = dict_find(iter,MESSAGE_KEY_tideData);

  if(tideDataTuple){
    char *tide = tideDataTuple->value->cstring;
    int tideData[HOURS_IN_DAY];
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Data received tide.c");    
    TideStruct dataToSave;
    dataToSave.date = getCurrentDate();
    
    //convert string data to float
    for(int i = 0; i < HOURS_IN_DAY; i++){
      tideFtData[i] = 10*(tide[0+i*MAX_DATA_PACKET_LENGTH]-'0')+1*(tide[1+i*MAX_DATA_PACKET_LENGTH]-'0');
      dataToSave.tideData[i] = (int)tideFtData[i];//to save in struct
      tideFtData[i] /= 10.0; 
    }
    saveTideDataToStorage(dataToSave);
    refreshGraphics();
  }
  
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  // A message was received, but had to be dropped
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped. Reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iter, void *context) {
  // The message just sent has been successfully delivered
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message sent to phone from watch");
}
  
static void outbox_failed_callback(DictionaryIterator *iter,
                                      AppMessageResult reason, void *context) {
  // The message just sent failed to be delivered
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message send failed. Reason: %d", (int)reason);
}

//TODO do something with the error codes
static bool loadTideDataFromStorage(TideStruct * tData){  
  int errorCode = persist_read_data(MESSAGE_KEY_TIDE_LOCAL_STORAGE,tData,sizeof(TideStruct));
  APP_LOG(APP_LOG_LEVEL_ERROR, "SAVING to local db.\nError code: %d\nreadDate: %d\ntide[0]: %d",errorCode,tData->date,tData->tideData[0]);
  return true;
}

//TODO do something wih the error codes
static bool saveTideDataToStorage(TideStruct tData){  
  int errorCode = persist_write_data(MESSAGE_KEY_TIDE_LOCAL_STORAGE,&tData,sizeof(TideStruct));   
  APP_LOG(APP_LOG_LEVEL_ERROR, "WRITING to local db.\nError code: %d\nreadDate: %d\ntide[0]: %d",errorCode,tData.date,tData.tideData[0]);
  APP_LOG(APP_LOG_LEVEL_ERROR, "writing sizeof: %d",sizeof(TideStruct));
  return true;
}

static void drawErrorGraphics(){
  //Draws a giant square
  for(int i = 0; i < HOURS_IN_DAY; i++){
    tideFtData[i] = 12;
  }
  refreshGraphics();
}


//send message to phone to send server request for new tide info
static bool sendToPhone(){
  // Declare the dictionary's iterator
  DictionaryIterator *out_iter;

  // Prepare the outbox buffer for this message
  AppMessageResult result = app_message_outbox_begin(&out_iter);
  if(result == APP_MSG_OK) {
    // Add an item to ask for tide data
    int value = 1234;
    dict_write_int(out_iter, MESSAGE_KEY_sendPhone, &value, sizeof(int), true);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to send to phone");
    // Send this message
    result = app_message_outbox_send();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent to phone");

    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
      return false;
    }else{
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Success sending: %d", (int)result);
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
    return false;
  }
  return true;
}  

//use getTideData when app starts up and at midnight
static bool getTideData(){
  
  //get current date 01-365  
  int currentDate = getCurrentDate();
  
  TideStruct tData;
  loadTideDataFromStorage(&tData);
  
  /*TEST - this can test the functionality when the 
   *watch hits midnight
   *currentDate = 2;
   */
  
  
  //make a call to the JS server because saved data is not current
  if(tData.date != currentDate){
    APP_LOG(APP_LOG_LEVEL_ERROR, "Local Data is old, need to make a server call\nCurrent Date %d Local DB Date: %d",currentDate,tData.date);
    drawErrorGraphics();//in this case these or more like loading graphics
    sendToPhone();

  //use saved data
  }else{
    //convert string data to float
    for(int i = 0; i < HOURS_IN_DAY; i++){
      tideFtData[i] = tData.tideData[i]/10.0;
    }
  }
  return true;
}

static int getCurrentDate(){
  char buffer[4];
  time_t rawtime;
  struct tm * timeinfo;
  int date = 0;
  
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer,4,"%j",timeinfo);
  date = 100*(buffer[0]-'0')+10*(buffer[1]-'0')+1*(buffer[2]-'0'); 
  return date;
}

static void clearGraphicData(){
  //free memory allcated for paths
  if(tide_path != NULL){
      gpath_destroy(tide_path);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Gpath_destroyed");
      tide_path = NULL;   
  }
}


static void drawText(GContext *ctx, const char * text,int x,int y,int width, int height){
//add numbers to tide graph
  GRect frame = GRect(x,y,width,height);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, 
    text,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    frame,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL
  );
}

//calculate the ceiling of a/b
static int ceiling(int a, int b){
  return (a + b - 1) / b;
}


static void drawCurrentTimeLineAndFeetLine(GContext *ctx){
  
  //get system time in format 14:33
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer,80,"%R",timeinfo);
  
  if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW CURRENT TIME LINE START----------");

  double timeInPX = 1000*(
  buffer[0]-'0')+100*(buffer[1]-'0')+10*(buffer[3]-'0')+1*(buffer[4]-'0');
  timeInPX/=100;//move two decimal places
  timeInPX*=PX_PER_HOUR;//each hour times by PX_PER_HOUR
  timeInPX+=5;//to account for shift of line in graphics
  
  //draw line of current time
  graphics_context_set_stroke_color(ctx,GColorBlack);
  graphics_context_set_stroke_width(ctx, PX_OF_TIME_LINE);
  GPoint verticalBottom = GPoint((int)timeInPX,HEIGHT_OF_GUI-8);
  GPoint verticalTop = GPoint((int)timeInPX,5);
  graphics_draw_line(ctx, verticalBottom, verticalTop);//vertical line
  
  //Draw line of current tide
  int x1,x2,x3,x4,y1,y2,y3,y4,i;
  x1 = x2 = x3 = x4 = y1 = y2 = y3 = y4 = i =  0;
  for(; i < HOURS_IN_DAY+2;i++){
    if(extremaPoints[i].x > verticalBottom.x){
      x1 = extremaPoints[i-1].x;
      x2 = extremaPoints[i].x;
      x3 = verticalBottom.x;
      x4 = verticalTop.x;
      y1 = extremaPoints[i-1].y;
      y2 = extremaPoints[i].y;
      y3 = verticalBottom.y;
      y4 = verticalTop.y;
      break;
    }
  }
  GPoint intersection = GPoint(((x1*y2-y1*x2)*(x3-x4)-(x1-x2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4)),
                               ((x1*y2-y1*x2)*(y3-y4)-(y1-y2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4)));

  GPoint p3 = GPoint(5,intersection.y);
  graphics_draw_line(ctx, p3, intersection);//horizontal line
  
 
  //write the current value of the tide next to the vertical time bar
  char str[10];
  int stringIndex = 0;
  if((MAX_FEET-ceiling(intersection.y,PX_PER_FOOT)+'0') < 0){
      str[stringIndex] = '-';
      stringIndex++;
  }
  str[stringIndex] = (ZERO_LINE_GUI-intersection.y)/PX_PER_FOOT+'0';
  
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"PX_PER_FOOT %d",PX_PER_FOOT);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"intersection.y %d",intersection.y);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"ZERO_LINE_GUI %d",ZERO_LINE_GUI);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"currentTide %d",(ZERO_LINE_GUI-intersection.y)/PX_PER_FOOT);
  }
  
  stringIndex++;
  str[stringIndex] = '.';
  stringIndex++;
  if((ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT > 9 || (ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT < -9){
    str[stringIndex] = '9';
  }else{
    str[stringIndex] = (ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT+'0';
  }
  stringIndex++;
  str[stringIndex] = 'f';
  stringIndex++;
  str[stringIndex] = 't';
  stringIndex++;
  str[stringIndex] = '\0';
    
  //if vertical bar is too far to the right draw tide to the left
  if(verticalTop.x > WIDTH_OF_GUI-50 || verticalTop.x > 70 ){
      drawText(ctx,str,verticalTop.x-30,verticalTop.y,30,20);
  //else draw bar to the right
  }else{
      drawText(ctx,str,verticalTop.x+5,verticalTop.y,30,20);
  }
  
  if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW CURRENT TIME LINE DONE----------");

}

static void drawOutlineOfGraph(GContext *ctx){
  //draw outline of tide graph
  GPoint p0 = GPoint(5,5);//8ft
  GPoint p1 = GPoint(5,ZERO_LINE_GUI);//0ft
  GPoint p2 = GPoint(140,ZERO_LINE_GUI);//12pm
  GPoint p3 = GPoint(5,132);//-2ft

  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, p0, p3);//vertical line
  graphics_draw_line(ctx, p1, p2);//horizontal line

  //draw feet  
  drawText(ctx,"8",18,(MAX_FEET-8)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawText(ctx,"6",18,(MAX_FEET-6)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawText(ctx,"4",18,(MAX_FEET-4)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawText(ctx,"2",18,(MAX_FEET-2)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawText(ctx,"0",18,(MAX_FEET-0)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawText(ctx,"2",18,(MAX_FEET+2)*PX_PER_FOOT,14,PX_PER_FOOT);
  
  //draw thick tick marks FT
  graphics_context_set_stroke_color(ctx,GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-8)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-8)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-6)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-6)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-4)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-4)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-2)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-2)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-0)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-0)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET+2)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET+2)*PX_PER_FOOT+PX_PER_FOOT/2+3));

  //draw thin tick marks FT
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-7)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-7)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-5)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-5)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-3)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-3)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET-1)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET-1)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  graphics_draw_line(ctx, GPoint(4,(MAX_FEET+1)*PX_PER_FOOT+PX_PER_FOOT/2+3), GPoint(14,(MAX_FEET+1)*PX_PER_FOOT+PX_PER_FOOT/2+3));
  
  //draw time
  drawText(ctx,"3",PX_PER_HOUR*3+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"6",PX_PER_HOUR*6+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"9",PX_PER_HOUR*9+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"N",PX_PER_HOUR*12+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"3",PX_PER_HOUR*15+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"6",PX_PER_HOUR*18+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"9",PX_PER_HOUR*21+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
}

static void drawHourlyTidePoints(GContext *ctx){
    graphics_context_set_stroke_color(ctx,tideBackgroundColor);
    graphics_context_set_stroke_width(ctx, 2);
    
    //calculate GPoints of hourly tide
    for(int i = 0; i < HOURS_IN_DAY; i++){
      int time = i;
      extremaPoints[i] = GPoint(TIME_STARTING_OFFSET+time*PX_PER_HOUR,ZERO_LINE_GUI-tideFtData[i]*PX_PER_FOOT);
    }
  
    //Draw Outline
    for(int i = 0; i < HOURS_IN_DAY-1; i++){
        graphics_draw_line(ctx,extremaPoints[i],extremaPoints[i+1]);
    }
}

static void fillInGraph(GContext *ctx){
    //fill in line w/ polygon
    GPoint startPoint = GPoint(5,ZERO_LINE_GUI);
    GPoint endPoint = GPoint(WIDTH_OF_GUI+5,ZERO_LINE_GUI);
    
    GPathInfo polygon = {.num_points = HOURS_IN_DAY+2,.points = (GPoint []){startPoint,extremaPoints[0],extremaPoints[1],extremaPoints[2],
                                                                                     extremaPoints[3],extremaPoints[4],extremaPoints[5],extremaPoints[6],
                                                                                     extremaPoints[7],extremaPoints[8],extremaPoints[9],extremaPoints[10],
                                                                                     extremaPoints[11],extremaPoints[12],extremaPoints[13],extremaPoints[14],
                                                                                     extremaPoints[15],extremaPoints[16],extremaPoints[17],extremaPoints[18],
                                                                                     extremaPoints[19],extremaPoints[20],extremaPoints[21],extremaPoints[22],
                                                                                     extremaPoints[23],endPoint}};
    graphics_context_set_fill_color(ctx, tideBackgroundColor);
    tide_path = gpath_create(&polygon);
    gpath_draw_filled(ctx, tide_path);
}

static void graph_update(Layer *layer, GContext *ctx) {      
    drawHourlyTidePoints(ctx);
    fillInGraph(ctx);
    drawOutlineOfGraph(ctx);
    drawCurrentTimeLineAndFeetLine(ctx);//update tide pin point lines 
}

/* Redraw the graph. This is used as an update method for when the time
 * or date changes
 */
static void refreshGraphics(){
    if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "refresh");
    clearGraphicData();
    layer_mark_dirty(graphic_layer);//this will redraw the graph layer
}

static void window_unload(Window *window) {
  if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "window unload");
  //We will safely destroy the Window's elements here!
  text_layer_destroy(clock_time);//destroy clock
  layer_destroy(graphic_layer);//destroy graph
}

static void window_load(Window *window){
  if(DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "window load");
  Layer *window_layer = window_get_root_layer(window);
  graphic_layer = layer_create(GRect(0, 0 , 144, 150));
  layer_set_update_proc(graphic_layer, graph_update);
  layer_add_child(window_layer, graphic_layer);
  
  // Create and Add to layer hierarchy:
  clock_time = text_layer_create(GRect(5,HEIGHT_OF_GUI, 144, 30));
  text_layer_set_text(clock_time, "");
  text_layer_set_font(clock_time,fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(clock_time));
  
  getTideData();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    if(DEBUG)APP_LOG(APP_LOG_LEVEL_DEBUG, "----------- Update Time ----------");
  
  // Write the current hours and minutes into a buffer
  static char s_buffer[50];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M  %h %d" : "%I:%M  %h %d", tick_time);
  // Display this time on the TextLayer
  text_layer_set_text(clock_time, s_buffer);
  
  //TODO only update graphics every 10 min
  //if(tick_time->tm_min % 10 == 0) {
    refreshGraphics();
  //}
  
  //this will update the new tide data and make a server call when it is 0000 in military time
  if(tick_time->tm_min == 0 && tick_time->tm_hour == 0){
    getTideData();
  }
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(MAX_LENGTH_JS_BUFFER_IN,MAX_LENGTH_JS_BUFFER_OUT);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  //update clock every minute
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  tideBackgroundColor = GColorFromRGB(0, 170, 170);//GColorFromRGB(0, 255, 255);//TODO possibly let user change this value
    
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  clearGraphicData();
  window_destroy(window);// Destroy Window
}

int main(void) {
  init();//setup gui
  app_event_loop();
  deinit();
}