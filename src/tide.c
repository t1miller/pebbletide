#include <pebble.h>

#define KEY_BACKGROUND_COLOR 0
#define MAX_NUMBER_TIDE_SWINGS 24

static void update_time();
static void load_resource();
static void inbox_received_handler(DictionaryIterator *iter, void *context);
static void clear();
static void getCurrentDate();
static int compareDates(char m1,char m2,char d1,char d2);
static void getSubString(int startingIndex);

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
static const bool DEBUG = false;

//Graphics
static Window *window;
static Layer *graphic_layer;
static TextLayer *clock_time;
static GPath *tide_path = NULL;//for filling in tide line
static GColor tideBackgroundColor;
static GPoint extremaPoints[MAX_NUMBER_TIDE_SWINGS];


//Data
static char* s_buffer;//from resource
static char dateBuffer[80];
static char year[4] = {'2','0','1','6'};
static char day[2] = {'0','1'};
static char month[2] = {'0','1'};
static char tideText[2000];
static char tideTextLength;
static char tideTextDivided[MAX_NUMBER_TIDE_SWINGS][31];
static float tideExtrema [MAX_NUMBER_TIDE_SWINGS] = {-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,
                                 -10,-10,-10,-10,-10,-10,-10,-10,-10,-10,
                                 -10,-10,-10,-10};
static float timeExtrema [MAX_NUMBER_TIDE_SWINGS] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                                 -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                                 -1,-1,-1,-1};


int locationName = 0;

//taken from http://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
char *itoa (int value, char *result, int base){
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

//This receives the message passed by the javascript portion
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  // High contrast selected?
  Tuple *background_color = dict_find(iter, KEY_BACKGROUND_COLOR);
  
  if(background_color) {  // Read boolean as an integer
    persist_write_int(KEY_BACKGROUND_COLOR,background_color->value->int32);
    tideBackgroundColor = GColorFromHEX(background_color->value->int32);
    update_time();
  }
}

static void load_resource() {
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"--------------LOADING RESOURCES START----------");
  }
  
  int startOffset, endOffset;
  
  getCurrentDate();
  // Get resource and size
  ResHandle handle = resource_get_handle(RESOURCE_ID_SanDiego2016);    
  size_t res_size = resource_size(handle);
  
  //only load resources for a few months not all months
  int currentMonth = 10*(month[0]-'0')+1*(month[1]-'0');
  
  if(currentMonth == 1 || currentMonth == 2)
    startOffset = 0;
  else
    startOffset = res_size*(currentMonth-2)/12;
  
  if(currentMonth == 12)
    endOffset = res_size;
  else
    endOffset = res_size*(currentMonth)/12;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG,"res_size = %d startOffset = %d endOffset = %d month = %d",(int)res_size,startOffset,endOffset,currentMonth);
  
  // Copy to buffer
  s_buffer = (char*)malloc(endOffset-startOffset);
  resource_load_byte_range(handle,startOffset,(uint8_t*)s_buffer,endOffset-startOffset);
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"--------------LOADING RESOURCES DONE----------");
  }
}

static void clear(){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"---------------CLEAR START------------");
  }
  
  for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS; i++){
    tideExtrema[i] = -10;
    timeExtrema[i] = -1;
  }  
  //free memory allcated for paths
  if(tide_path != NULL){
      gpath_destroy(tide_path);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Gpath_destroyed");
      tide_path = NULL;   
  }
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"---------------CLEAR DONE------------");
  }
}

//get the date from the system
static void getCurrentDate(){
  time_t rawtime;
  struct tm * timeinfo;
  
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"-------------GET CURRENT DATE START----------");
  }
  
  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (dateBuffer,80,"%F",timeinfo);//get the date in format 2015-12-14
  year[0] = dateBuffer[0];
  year[1] = dateBuffer[1];
  year[2] = dateBuffer[2];
  year[3] = dateBuffer[3];
  month[0] = dateBuffer[5];
  month[1] = dateBuffer[6];
  day[0] = dateBuffer[8];
  day[1] = dateBuffer[9];
  
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"-------------GET CURRENT DATE DONE----------");
  }
}

//2015-12-14
//if current date < date2 return 1
//else current date > date2 return 2
//else return 3
static int compareDates(char m1,char m2,char d1,char d2){
  int currentMonth = (month[0]-'0')*10 + (month[1] - '0')*1;
  int month2 = (m1-'0')*10 + (m2 - '0')*1;
  int currentDay = (day[0]-'0')*10 + (day[1] - '0')*1;
  int day2 = (d1-'0')*10 + (d2 - '0')*1;
  
  if( currentMonth < month2 ){
    return 1;
  }else if( currentMonth > month2){
    return 2;
  }else{
    if(currentDay < day2){
      return 1;
    }else if(currentDay > day2){
      return 2;
    }else{
      return 3;
    }
  }
}

//after we have found the date this will get
//the beginning part of the string and the ending
//part
static void getSubString(int startingIndex){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------GETTING SUBSTRING START------------");
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Getting Substring index %d",startingIndex);
  }
  
  int newStartingPoint = 0;
  int newEndingPoint = 0;
  int current = startingIndex;
  
  //for(int i = 0; i < 2000; i++)
   // tideText[i] = '\0';
  
  //find where the date begins
  //keep searching up until you found a date < the current date
  while(1){
    if(s_buffer[current]!='\n'){
      current--;
    }else{
      if(compareDates(s_buffer[current+6],s_buffer[current+7],
                      s_buffer[current+9],s_buffer[current+10]) == 3){
        newStartingPoint = current;
        current--;
      }else{
        break;
      }
    }
  }
      
  current = startingIndex;
  //find where the date ends
  //keep searching down until you find a date > the current date
  while(1){  
    if(s_buffer[current]!='\n'){
      current++;
    }else{
      if(compareDates(s_buffer[current+6],s_buffer[current+7],
                      s_buffer[current+9],s_buffer[current+10]) 
                      == 1){
        newEndingPoint = current;
        break;
      }else{
        current++;
      }
    }
  }
  
  /*partial copy*/
  strncpy (tideText,&s_buffer[newStartingPoint+1],newEndingPoint-newStartingPoint);
  tideText[newEndingPoint-newStartingPoint] = '\0';   /* null character manually added */
  tideTextLength = newEndingPoint-newStartingPoint;
  
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------GETTING SUBSTRING END------------");
  } 
}

//this is performing a binary search for the date
//this will return the s_buffer index
//that the string was found
static int search(){
  
  unsigned int high = strlen(s_buffer);
  unsigned int low = 0;
  unsigned int current = (high - low)/2;

  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------SEARCH START------------");
  }
  while((high-low) >= 1){

      //we found a date
      //and are not at an ending point
      if(s_buffer[current]=='/'){
            //make sure we are at the proper bracket 2015/12/20
            //                                      here ^
            if(s_buffer[current + 3] == ' '){
              current -=3;  
            //we found the date
            }else if(compareDates(s_buffer[current+1],s_buffer[current+2],s_buffer[current+4],s_buffer[current+5]) == 3){
              if(DEBUG){
                APP_LOG(APP_LOG_LEVEL_DEBUG, "we found it: %c - /%c%c/%c%c",s_buffer[current],s_buffer[current+1],s_buffer[current+2],s_buffer[current+4],s_buffer[current+5]);
                APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------SEARCH DONE------------");
              }
              return current;
            //the date found is too small
            }else if(compareDates(s_buffer[current+1],s_buffer[current+2],s_buffer[current+4],s_buffer[current+5]) == 2){
              low = current;
              current = (high + low)/2;
            //the date we found is too big
            }else{
              high = current;
              current = (high + low)/2;
            }
      }else{
        current++;
      }
  }
  return 0;
}

/*given a substring turn it into a bunch
 of smaller strings*/
static void divideUpSubString(){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"-------------DIVIDING SUBSTRING START-----------");
  }  
  int z = 0;
  for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS; i++){
      for(int k = 0; k < 32; k++){
        tideTextDivided[i][k] = tideText[z];
        z++;
        if(tideTextDivided[i][k] == '\n'){
          tideTextDivided[i][k+1] = '\0';
          break;
        }
      }
  }
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"-------------DIVIDING SUBSTRING DONE-----------");
  }
}

static void getExtremaFromText(){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"----------GETTING EXTREMA START------------");
  }
  
  int i = 0;
  while(i < MAX_NUMBER_TIDE_SWINGS){
    //get time in px
    timeExtrema[i] = 1000*(tideTextDivided[i][15]-'0')+100*(tideTextDivided[i][16]-'0')+10*(tideTextDivided[i][18]-'0')+1*(tideTextDivided[i][19]-'0');
    
    //convert time to military
    //take care of corner cases
    if(tideTextDivided[i][21] == 'P')
      timeExtrema[i] += 1200;
    if((tideTextDivided[i][21] == 'P' && tideTextDivided[i][15] == '1' && tideTextDivided[i][16] == '2' ) || (tideTextDivided[i][21] == 'A' && tideTextDivided[i][15] == '1' && tideTextDivided[i][16] == '2'))
      timeExtrema[i] -= 1200;
    
    timeExtrema[i] /= 100;
    
    //get tide
    //take care of negative case
    if(tideTextDivided[i][24] == '-'){
      tideExtrema[i] = 10*(tideTextDivided[i][25]-'0') + 1*(tideTextDivided[i][27]-'0');
      tideExtrema[i] *= -1;
    }else{
      tideExtrema[i] = 10*(tideTextDivided[i][24]-'0') + 1*(tideTextDivided[i][26]-'0');
    }
    tideExtrema[i] /= 10;
    
    i++;   
  }
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"----------GETTING EXTREMA DONE------------");
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
  
  if(DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW CURRENT TIME LINE START----------");

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
  for(; i < MAX_NUMBER_TIDE_SWINGS+2;i++){
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
  if((ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT > 9 || (ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT < -9)
    str[stringIndex] = '9';
  else
    str[stringIndex] = (ZERO_LINE_GUI-intersection.y)%PX_PER_FOOT+'0';
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
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW CURRENT TIME LINE DONE----------");

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

static void drawExtremaPoints(GContext *ctx){
    graphics_context_set_stroke_color(ctx,tideBackgroundColor);
    graphics_context_set_stroke_width(ctx, 2);
  
    if(DEBUG)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW EXTREMA POINTS START----------");
  
    //calculate GPoints of extrema
    for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS; i++){
      extremaPoints[i] = GPoint(TIME_STARTING_OFFSET+timeExtrema[i]*PX_PER_HOUR,ZERO_LINE_GUI-tideExtrema[i]*PX_PER_FOOT);
    }
  
    //Draw Outline
    for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS-1; i++){
      if(timeExtrema[i+1] != -1){  
        graphics_draw_line(ctx,extremaPoints[i],extremaPoints[i+1]);
      }
    }
    
    if(DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "-----------DRAW EXTREMA POINTS DONE----------");
  
}

static void fillInGraph(GContext *ctx){
    //fill in line w/ polygon
    GPoint startPoint = GPoint(5,ZERO_LINE_GUI);
    GPoint endPoint = GPoint(WIDTH_OF_GUI+5,ZERO_LINE_GUI);
    
    GPathInfo polygon = {.num_points = MAX_NUMBER_TIDE_SWINGS+2,.points = (GPoint []){startPoint,extremaPoints[0],extremaPoints[1],extremaPoints[2],
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
    getExtremaFromText();
    drawExtremaPoints(ctx);
    fillInGraph(ctx);
    drawOutlineOfGraph(ctx);
    drawCurrentTimeLineAndFeetLine(ctx);//update tide pin point lines 
}

/* Redraw the graph. This is used as an updated method for when the time
 * or date changes
 */
static void refresh(){
    if(DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------REFRESH START-----------");
  
    clear();
    getCurrentDate();
    getSubString(search());//get new date data
    divideUpSubString();//split up chunk of date data
    layer_mark_dirty(graphic_layer);//this will redraw the graph layer
    
    if(DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "-------------REFRESH DONE-----------");
}

static void window_unload(Window *window) {
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window unload");
  //We will safely destroy the Window's elements here!
  text_layer_destroy(clock_time);//destroy clock
  layer_destroy(graphic_layer);//destroy graph
}

static void window_load(Window *window){
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window load");
  Layer *window_layer = window_get_root_layer(window);
  graphic_layer = layer_create(GRect(0, 0 , 144, 150));
  layer_set_update_proc(graphic_layer, graph_update);
  layer_add_child(window_layer, graphic_layer);
  
  // Create and Add to layer hierarchy:
  clock_time = text_layer_create(GRect(5,HEIGHT_OF_GUI, 144, 30));
  text_layer_set_text(clock_time, "");
  text_layer_set_font(clock_time,fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(clock_time));
  
  //if a color exists load it
  if(persist_exists(KEY_BACKGROUND_COLOR)){
    tideBackgroundColor = GColorFromHEX(persist_read_int(KEY_BACKGROUND_COLOR));
  }else{
    tideBackgroundColor = GColorVividCerulean;//initialize color of tide graph
  }

}

static void update_time() {
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update time");
  
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[50];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M  %h %d" : "%I:%M  %h %d", tick_time);
  // Display this time on the TextLayer
  text_layer_set_text(clock_time, s_buffer);
  refresh();//update graphics with time
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void init(void) {
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init");
  
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(100,100);

  load_resource();

  //update clock every minute
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit");
  
  free(s_buffer);
  clear();
  window_destroy(window);// Destroy Window
}

int main(void) {
  if(DEBUG)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "main");
  init();//setup gui
  app_event_loop();
  deinit();
}