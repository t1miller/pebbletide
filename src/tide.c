/* Current Limitations(Bugs)
 * -The GUI is only designed for tide swings from 7ft to -2ft
 * -Only supports San Diego
 * -Only support 6 tide extrema per day (Because of drawing the graph to scale)
 * -The first and second line displaying the tide levels are not based on real data.
 *  It is only dependant on whether or not the tide extrema of the first or last point is 
 *  a min or max. Ideally we would get this data from the previous or next day but these are times
 *  when it is dark so I don't realy care.
 */
#include <pebble.h>

#define LATITUDE 1
#define LONGITUDE 2
#define NUMBER_OF_LOCATIONS 4

//Constants
static const int WIDTH_OF_GUI = 140;
static const int HEIGHT_OF_GUI = 135;
static const int ZERO_LINE_GUI = 89;//to account for offset of line thickness
static const int MAX_NUMBER_TIDE_SWINGS = 6;
static const float PX_PER_FOOT = 12;
static const float PX_THICKNESS_OF_RED_LINE = 4;
static const float PX_PER_HOUR = 5.7;
static const int PX_OF_TIME_LINE = 1;
static const bool DEBUG = true;

//Graphics
static Window *window;
static int numberOfTideSwings;
static Layer *graphic_layer;
static TextLayer *clock_time;
static GPath *s_my_path_ptr[7] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};//for filling in tide line
static GPoint extremaPoint[6+2];//to account for start and end

//Data
static char* s_buffer;//from resource
static char dateBuffer[80];
static char year[4] = {'2','0','1','6'};
static char day[2] = {'0','1'};
static char month[2] = {'0','1'};
static char tideText[300];
static char tideTextDivided[6][50];
static int tideExtrema[6] = {-1,-1,-1,-1,-1,-1};//for graph
static int timeofTideExtremaInPx[6] = {-1,-1,-1,-1,-1,-1};//for graph
int locationName = 0;

//check if a character is a digit or '-'
static bool isDigit(char ch){
  if((ch>='0' && ch<='9') || ch == '-'){
    return 1;
  }else{
    return 0;
  }
}

static void load_resource() {
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Loading Resources");
  }

  // Get resource and size
  ResHandle handle = resource_get_handle(RESOURCE_ID_SanDiego2016);  

  size_t res_size = resource_size(handle);

  // Copy to buffer
  s_buffer = (char*)malloc(res_size);
  resource_load(handle, (uint8_t*)s_buffer, res_size);
}

static void clear(){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Clear");
  }
  
  for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS; i++){
    tideExtrema[i] = -1;
    timeofTideExtremaInPx[i] = -1;
  }  
  //free memory allcated for paths
    for(int i = 0; i < numberOfTideSwings + 1; i++){
      if(s_my_path_ptr[i] != NULL){
        gpath_destroy(s_my_path_ptr[i]);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Gpath_destroy: %d",i);
        s_my_path_ptr[i] = NULL;
      }
    }
  numberOfTideSwings = 0;
}

//get the date from the system
static void getCurrentDate(){
  time_t rawtime;
  struct tm * timeinfo;

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
}

//Feb = 28
//Sep,April,June,Noveber = 30
//jan,march,may,july,august,october,december = 31
//doesnt take into account leap years
static void getNextDate(){
  //the day is between 1 and 8
  if(day[0] == '0' && day[1] != '9'){
    day[1] += 1;
  //the day is 9
  }else if(day[0] == '0' && day[1] == '9' ){
    day[0] = '1';
    day[1] = '0';
  //the day is between 10 and 18
  }else if(day[0] == '1' && day[1] != '9'){
    day[1]+=1;
  //the day is 19
  }else if(day[0] == '1' && day[1] == '9'){
    day[0] = '2';
    day[1] = '0';
  //the day is between 20 and 27
  }else if(day[0] == '2' && (day[1] != '8' && day[1] != '9')){
    day[1]+=1;  
  //the day is 28
  }else if(day[0] == '2' && day[1] == '8'){
    //february
    if(month[0] == '0' && month[1] == '2'){
      month[1] = '3';
      day[0] = '0';
      day[1] = '1';
    }else{
      day[0] = '2';
      day[1] = '9';
    }
  //the day is 29
  }else if(day[0] == '2' && day[1] == '9'){
    day[0] = '3';
    day[1] = '0';
  //the day is 30
  }else if(day[0] == '3' && day[1] == '0'){
    //april
    if(month[0] == '0' && month[1] == '4'){
      month[0] = '0';
      month[1] = '5';
      day[0] = '0';
      day[1] = '1';
    //june
    }else if(month[0] == '0' && month[1] == '6'){
      month[0] = '0';
      month[1] = '7';
      day[0] = '0';
      day[1] = '1';
    //Nov
    }else if(month[0] == '1' && month[1] == '1'){
      month[0] = '1';
      month[1] = '2';
      day[0] = '0';
      day[1] = '1';
    //Sep
    }else if(month[0] == '0' && month[1] == '9'){
      month[0] = '1';
      month[1] = '0';
      day[0] = '0';
      day[1] = '1';
    }else{
      day[0] = '3';
      day[1] = '1';   
    }
  //day is 31
  }else if(day[0] == '3' && day[1] == '1'){
    if(month[0] == '0' && month[1] != '9'){
      month[0] = '0';
      month[1] += 1;
      day[0] = '0';
      day[1] = '1';
    }else if(month[0] == '0' && month[1] == '9'){
      month[0] = '1';
      month[1] = '0';
      day[0] = '0';
      day[1] = '1';
    }else if(month[0] == '1' && month[1] == '0'){
      month[0] = '1';
      month[1] = '1';
      day[0] = '0';
      day[1] = '1';
    }else if(month[0] == '1' && month[1] == '1'){
      month[0] = '1';
      month[1] = '2';
      day[0] = '0';
      day[1] = '1';
    }else if(month[0] == '1' && month[1] == '2'){
      month[0] = '0';
      month[1] = '1';
      day[0] = '0';
      day[1] = '1';
    }
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
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Getting Substring");
  }
  
  int newStartingPoint = 0;
  int newEndingPoint = 0;
  int current = startingIndex;

  for(int i = 0; i < 300; i++)
    tideText[i] = '\0';
  
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
}

//this is performing a binary search for the date
//this will return the in the s_buffer
//that the string was found
static int search(){
  unsigned int high = strlen(s_buffer);
  unsigned int low = 0;
  unsigned int current = (high - low)/2;

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
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Dividing Substring");
  }

  int i = 0;
  int j = 0;
  int k = 0;
  while(tideText[i]!='\0'){
    //a date is found
    if(tideText[i] == '/'){
      //copy important information
      for(k = 0; k < 22; k++){
        tideTextDivided[j][k+1] = '\0';
        tideTextDivided[j][k] = tideText[i+10+k];
        
        if(tideText[i+10+k] == 'H' || tideText[i+10+k] == 'L'){//stop copying when you hit the end
          numberOfTideSwings++;
          //get rid of cm
          if(isDigit(tideTextDivided[j][k-2])){
            tideTextDivided[j][k-2] = ' ';
            if(isDigit(tideTextDivided[j][k-3])){
              tideTextDivided[j][k-3] = ' ';
              if(isDigit(tideTextDivided[j][k-4])){
                tideTextDivided[j][k-4] = ' ';
                if(isDigit(tideTextDivided[j][k-5])){
                  tideTextDivided[j][k-5] = ' ';
                }
              }
            }
          }
          break;
        }
      }
      i+=21;//skip all the information we copied
      j++;//fill in the next date
    }
    i++;
  }
}

static void getMinAndMaxTidesAtTimes(){
  if(DEBUG){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Getting Min and Max");
  }

  int i = 0;
  while(i < numberOfTideSwings){
    //get time in px
    timeofTideExtremaInPx[i] = 1000*(tideTextDivided[i][1]-'0')+100*(tideTextDivided[i][2]-'0')+10*(tideTextDivided[i][4]-'0')+1*(tideTextDivided[i][5]-'0');
    
    //convert to military
    //take care of corner cases
    if(tideTextDivided[i][7] == 'P')
      timeofTideExtremaInPx[i]+=1200;
    if((tideTextDivided[i][7] == 'P' && tideTextDivided[i][1] == '1' && tideTextDivided[i][2] == '2' ) || (tideTextDivided[i][7] == 'A' && tideTextDivided[i][1] == '1' && tideTextDivided[i][2] == '2'))
      timeofTideExtremaInPx[i]-=1200;
    //convert to px
    timeofTideExtremaInPx[i]/=100;//move two decimal places
    timeofTideExtremaInPx[i]*=PX_PER_HOUR;//each hour times by 5
    timeofTideExtremaInPx[i]+=5;//to account for shift of line in graphics
    
    //tideExtrema
    //take care of negative case
    if(tideTextDivided[i][10] == '-'){
      tideExtrema[i]=0;
      tideExtrema[i]=70-10*(tideTextDivided[i][11]-'0') + 1*(tideTextDivided[i][13]-'0');
    }else{
      tideExtrema[i]=0;
      tideExtrema[i]=70-10*(tideTextDivided[i][10]-'0') + 1*(tideTextDivided[i][12]-'0');
    }
    
    tideExtrema[i]*=PX_PER_FOOT;
    tideExtrema[i]/=10;
    i++;
  }
}

static void drawPoints(GContext *ctx){
  
    if(DEBUG){
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Drawing Points");
    }


    float endingSlope = 0;//to determine if last tide is going down or up

    graphics_context_set_stroke_color(ctx,GColorVividCerulean);
    graphics_context_set_stroke_width(ctx, 2);
    
    for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS-1; i++){
      if(tideExtrema[i+1] != -1){
        extremaPoint[i+1] = GPoint(timeofTideExtremaInPx[i],tideExtrema[i]);
        extremaPoint[i+2] = GPoint(timeofTideExtremaInPx[i+1],tideExtrema[i+1]);
        graphics_draw_line(ctx, extremaPoint[i+1], extremaPoint[i+2]);//vertical line
        //100 is an arbitrary constant to get a better spread
        endingSlope = 100*(extremaPoint[i+2].y - extremaPoint[i+1].y)/(extremaPoint[i+2].x-extremaPoint[i+1].x);
      }  
    }

    //get starting line
    if(tideExtrema[0]>tideExtrema[1]){
      extremaPoint[0] = GPoint(0,tideExtrema[0]-35);
    }else{
      extremaPoint[0] = GPoint(0,tideExtrema[0]+35);
    }
    graphics_draw_line(ctx,extremaPoint[0], extremaPoint[1]);//vertical line

    //get end line
    if(endingSlope <= 0){
      extremaPoint[numberOfTideSwings+1] = GPoint(WIDTH_OF_GUI+10,tideExtrema[numberOfTideSwings-1]+35);
    }else{
      extremaPoint[numberOfTideSwings+1] = GPoint(WIDTH_OF_GUI+10,tideExtrema[numberOfTideSwings-1]-35);
    }
    graphics_draw_line(ctx,extremaPoint[numberOfTideSwings],extremaPoint[numberOfTideSwings+1]);//vertical line
        
    //fill in lines w/ polygons
    //TODO error here
    GPathInfo polygon [7] ={ {.num_points = 4,.points = (GPoint []) {extremaPoint[0],{extremaPoint[1].x+1,extremaPoint[1].y+1},{extremaPoint[1].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[0].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[1],{extremaPoint[2].x+1,extremaPoint[2].y+1},{extremaPoint[2].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[1].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[2],{extremaPoint[3].x+1,extremaPoint[3].y+1},{extremaPoint[3].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[2].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[3],{extremaPoint[4].x+1,extremaPoint[4].y+1},{extremaPoint[4].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[3].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[4],{extremaPoint[5].x+1,extremaPoint[5].y+1},{extremaPoint[5].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[4].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[5],{extremaPoint[6].x+1,extremaPoint[6].y+1},{extremaPoint[6].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[5].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}},
                             {.num_points = 4,.points = (GPoint []) {extremaPoint[6],{extremaPoint[7].x+1,extremaPoint[7].y+1},{extremaPoint[7].x+1,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1},{extremaPoint[6].x,ZERO_LINE_GUI-PX_THICKNESS_OF_RED_LINE+1}}}
                           };

    graphics_context_set_fill_color(ctx, GColorVividCerulean);
    for(int i = 0; i < numberOfTideSwings+1 ; i++){
      
      s_my_path_ptr[i] = gpath_create(&polygon[i]);
      gpath_draw_filled(ctx, s_my_path_ptr[i]);
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

//The text for the feet should be right aligned
static void drawTextFeet(GContext *ctx, const char * text,int x,int y,int width, int height){
//add numbers to tide graph
  GRect frame = GRect(x,y,width,height);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, 
    text,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    frame,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentRight,
    NULL
  );
}

static void drawCurrentTimeLineAndFeetLine(GContext *ctx){
  
  //get system time in format 14:33
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer,80,"%R",timeinfo);
  
  double timeInPX = 1000*(buffer[0]-'0')+100*(buffer[1]-'0')+10*(buffer[3]-'0')+1*(buffer[4]-'0');
  timeInPX/=100;//move two decimal places
  timeInPX*=PX_PER_HOUR;//each hour times by PX_PER_HOUR
  timeInPX+=5;//to account for shift of line in graphics
  
  //draw line of current time
  graphics_context_set_stroke_color(ctx,GColorBlack);
  graphics_context_set_stroke_width(ctx, PX_OF_TIME_LINE);
  GPoint p1 = GPoint((int)timeInPX,HEIGHT_OF_GUI-8);
  GPoint p2 = GPoint((int)timeInPX,5);
  graphics_draw_line(ctx, p1, p2);//vertical line
  
  
  //Draw line of current tide
  int x1,x2,x3,x4,y1,y2,y3,y4;
  x1 = x2 = x3 = x4 = y1 = y2 = y3 = y4 = 0;
  for(int i = 0; i < MAX_NUMBER_TIDE_SWINGS+2;i++){
    if(extremaPoint[i].x > p1.x){
      x1 = extremaPoint[i-1].x;
      x2 = extremaPoint[i].x;
      x3 = p1.x;
      x4 = p2.x;
      y1 = extremaPoint[i-1].y;
      y2 = extremaPoint[i].y;
      y3 = p1.y;
      y4 = p2.y;
      break;
    }
  }
  GPoint intersection = GPoint(((x1*y2-y1*x2)*(x3-x4)-(x1-x2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4)),
                               ((x1*y2-y1*x2)*(y3-y4)-(y1-y2)*(x3*y4-y3*x4))/((x1-x2)*(y3-y4)-(y1-y2)*(x3-x4)));

  GPoint p3 = GPoint(5,intersection.y);
  graphics_draw_line(ctx, p3, intersection);//vertical line
}

static void drawOutlineOfGraph(GContext *ctx){
  //draw outline of tide graph
  //every ft in tide is = 12 px
  //''    time   ''  '' = 5 px 
  GPoint p0 = GPoint(5,5);//7ft
  GPoint p1 = GPoint(5,ZERO_LINE_GUI);//0ft
  GPoint p2 = GPoint(140,ZERO_LINE_GUI);//12pm
  GPoint p3 = GPoint(5,125);//-2ft

  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, p0, p3);//vertical line
  graphics_draw_line(ctx, p1, p2);//horizontal line
  
  //add numbers to tide graph
  drawTextFeet(ctx,"+7",8,(7-7)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+6",8,(7-6)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+5",8,(7-5)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+4",8,(7-4)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+3",8,(7-3)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+2",8,(7-2)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+1",8,(7-1)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"+0",8,(7-0)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"-1",8,(7+1)*PX_PER_FOOT,14,PX_PER_FOOT);
  drawTextFeet(ctx,"-2",8,(7+2)*PX_PER_FOOT,14,PX_PER_FOOT);
  
  //add times
  drawText(ctx,"3",PX_PER_HOUR*3+PX_THICKNESS_OF_RED_LINE+1/*easier on the eyes*/,ZERO_LINE_GUI,10,7);
  drawText(ctx,"6",PX_PER_HOUR*6+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"9",PX_PER_HOUR*9+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"N",PX_PER_HOUR*12+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"3",PX_PER_HOUR*15+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"6",PX_PER_HOUR*18+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
  drawText(ctx,"9",PX_PER_HOUR*21+PX_THICKNESS_OF_RED_LINE,ZERO_LINE_GUI,10,7);
}

static void graph_update(Layer *layer, GContext *ctx) {  
    
    getMinAndMaxTidesAtTimes();//get extrema
    drawPoints(ctx);//draw extrema
    drawCurrentTimeLineAndFeetLine(ctx);//update tide pin point lines

    //draw outline of the graph if it isn't already drawn
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Drawing Outline");
  
    drawText(ctx,"SAN",100,0,80,14);//draw tide location i.e LA
  
    drawOutlineOfGraph(ctx);
}

/* Redraw the graph. This is used as an updated method for when the time
 * or date changes
 */
static void refresh(){
  if(DEBUG)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Refresh Called");
  
    clear();
    getCurrentDate();
    getSubString(search());//get new date data
    divideUpSubString();//split up chunk of date data
    layer_mark_dirty(graphic_layer);//this will redraw the graph layer
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

  //update clock every minute
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
  
  load_resource();
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