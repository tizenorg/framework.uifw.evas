
#include "evas_common.h"
#include "evas_private.h"
#include "Evas.h"

#define LOG_LEN_MAX 1024
#define null NULL

static void dump_layer(Evas_Layer* layer, FILE* pFile);
static int  dump_object(Evas_Object* object,int depth, FILE* pFile);
static void dump_tree(Evas* evas, FILE* pFile);

#define PRINT(...)	fprintf(stderr,__VA_ARGS__)
//#define PRINT(...)

//#define DUMP_DEBUG
#ifdef DUMP_DEBUG
#define FOPEN(_RES, _MODE) fprintf(stderr,"fopen(%s,%s)",#_RES, #_MODE)
#define FPRINT(_FP, ...) fprintf(stderr,__VA_ARGS__)
#define FPUTS(_FP,__MESSAGE) fprintf(stderr,"%s",__MESSAGE)
#define FCLOSE(_FP)	fprintf(stderr,"fclose()")
#else
#define FOPEN(_RES, _MODE) fopen(_RES, _MODE)
#define FPRINT(_FP, ...) fprintf(_FP,__VA_ARGS__)
#define FPUTS(_FP,__MESSAGE) fprintf(_FP,"%s", __MESSAGE)
#define FCLOSE(_FP)	fclose(_FP)
#endif

static int object_depth = 0;

/************************************************************************************************
// property class
*************************************************************************************************/
//#define PRINT(...)  printf(__VA_ARGS__)
#define MAX_PROP 50
typedef struct _PROP_CLASS
{
  struct
  {
    char* key;
    char* value;
  }prop[MAX_PROP];
  int count;
  char* (*GetValue)(struct _PROP_CLASS* style, const char* key);
  char* (*GetValueByIndex)(struct _PROP_CLASS* style, int index);
  int (*SetValue)(struct _PROP_CLASS* style, const char* key, const char* value);
  char* (*GetKey)(struct _PROP_CLASS* style, int index);
  int (*GetCount)(struct _PROP_CLASS* style);
}_PropertyClass;

static char* _Propertyclass_GetValue(_PropertyClass* style, const char* key);
static int _PropertyClass_SetValue(_PropertyClass* style, const char* key, const char* value);
static char* _Propertyclass_GetValueByIndex(_PropertyClass* style, int index);
static char* _PropertyClass_GetKey(_PropertyClass* style, int index);
static int _PropertyClass_GetCount(_PropertyClass* style);

_PropertyClass*
_PropertyClass_New(void)
{
  _PropertyClass* pProperty = malloc(sizeof(_PropertyClass));
  memset(pProperty,0,sizeof(_PropertyClass));

  pProperty->GetValue = _Propertyclass_GetValue;
  pProperty->GetValueByIndex = _Propertyclass_GetValueByIndex;
  pProperty->SetValue = _PropertyClass_SetValue;
  pProperty->GetKey   = _PropertyClass_GetKey;
  pProperty->GetCount = _PropertyClass_GetCount;
  return pProperty;
}

int
_PropertyClass_Delete(_PropertyClass* Property)
{
  if(!Property)
    return 1;
  int i = 0;

  for(; i < MAX_PROP ; i++)
    {
      if(Property->prop[i].key)
        free(Property->prop[i].key);
      if(Property->prop[i].value)
        free(Property->prop[i].value);
    }
  free(Property);
  return 1;
}


static char*
_Propertyclass_GetValue(_PropertyClass* style, const char* key)
{
  int i = 0;

  if(key == NULL)
    return NULL;

  for(; i< MAX_PROP;i++)
    {
      if(style->prop[i].key && strcmp(style->prop[i].key, key) == 0)
        {
          return style->prop[i].value;
        }
    }
  return NULL;
}

static char*
_Propertyclass_GetValueByIndex(_PropertyClass* style, int index)
{
    return style->prop[index].value;
}


static int
_PropertyClass_SetValue(_PropertyClass* style, const char* key, const char* value)
{
  int i = 0;

  if(key == NULL)
    return -1;

  for(; i< MAX_PROP;i++)
    {
      if(style->prop[i].key && strcmp(style->prop[i].key, key) == 0)
        {

          if(style->prop[i].value)
            free(style->prop[i].value);
          style->prop[i].value = NULL;

          if(value != NULL)
            {
               style->prop[i].value = strdup(value);
            }
          return i;
        }
    }

  i = style->count;
  style->count++;

  style->prop[i].key = strdup(key);

  if(value != NULL)
    {
       style->prop[i].value = strdup(value);
    }

  return i;
}

static char*
_PropertyClass_GetKey(_PropertyClass* style, int index)
{
  return style->prop[index].key;
}

static int
_PropertyClass_GetCount(_PropertyClass* style)
{
  return style->count;
}



int
_PropertyClass_Print(_PropertyClass* prop)
{
  int i = 0;
  PRINT("property:count:%d\n",i, prop->count);
  for(i=0;i<prop->count;i++ )
    {
      PRINT("prop[%d]:%s\t\t= %s\n",i, prop->prop[i].key,prop->prop[i].value);
    }
}

int
_PropertyClass_PrintOut(_PropertyClass* prop,char* output, int output_size)
{
  int i = 0;

  char key_value[4096];
  for(i=0;i<prop->count;i++ )
    {
      sprintf(key_value, "-%s\t= %s\n",prop->prop[i].key,prop->prop[i].value);
      strncat(output, key_value, output_size);
    }
  PRINT("%s\n",output);
}

static _PropertyClass*
_get_textblock_property(const char* textblock_style)
{
  const char token[]=" :'";
  char* next = NULL,*style=NULL;

  const char* def_tag = "DEFAULT='";

  char *tmpbuff = strdup(textblock_style);


  PRINT("intput:%s\n",tmpbuff);

  style = strstr(tmpbuff, "DEFAULT='");

  if(!style)
    return NULL;

  _PropertyClass* pProperty = _PropertyClass_New();


  style = style + strlen(def_tag);
  PRINT("%s\n",style);
  char* end = strchr(style,'\'');

  if(end == NULL)
    end = strlen(style);

  char* prop_str = strtok_r(style, token, &next);


  char key[255],value[2048];


  while(prop_str)
    {
      if(end <= prop_str)
        break;

      char* tok = strchr(prop_str,'=');
      int keylen=0;
      int vallen = 0;
      if(tok)
        {
          keylen=tok-prop_str;
          strncpy(key, prop_str, keylen);
          key[keylen]=0;

          vallen = strlen(tok + 1);
          if (vallen >= sizeof(value)) vallen = sizeof(value) - 1;
          strncpy(value, tok + 1, vallen);
          value[vallen] = 0;
        }
      else
        {
          keylen = strlen(prop_str);
          strncpy(key, prop_str, keylen);
          key[keylen]=0;
          value[0]=0;
        }


      PRINT("%s\n -> key(%s):%s\n",prop_str, key,value);

      pProperty->SetValue(pProperty, key,value);

      prop_str = strtok_r(NULL, token, &next);
    }

  char* source = pProperty->GetValue(pProperty,"font_source");
  if(source)
    {
      char* tmp= strrchr(source,'/');
      char* filename = strdup(tmp+1);
      pProperty->SetValue(pProperty, "font_source", filename);

    }
//  pProperty->SetValue();

  return pProperty;
}



static void _get_text_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);
static void _get_textblock_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);
static void _get_textgrid_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);
static void _get_image_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);
static void _get_default_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);

typedef enum
{
	O_NONE,
	O_IMAGE,
	O_TEXT,
	O_TEXTBLOCK,
	O_TEXTGRID,
	O_CLIPPER,
	O_LINE,
	O_RECTANGLE,
	O_POLYGON,
	O_SMART,
	O_MAX,
}objectType;

typedef struct
{
	int objType;
	void (*getDesc)(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText);
}_ObjDescParser;

_ObjDescParser objParser[]=
{
	{O_NONE, NULL},
	{O_IMAGE, _get_image_desc},
	{O_TEXT, _get_text_desc},
    {O_TEXTBLOCK, _get_textblock_desc},
    {O_TEXTGRID, _get_textgrid_desc},
	{O_CLIPPER, NULL},
	{O_LINE, _get_default_desc},
	{O_RECTANGLE, _get_default_desc},
	{O_POLYGON, _get_default_desc},
	{O_SMART, NULL},
	{O_MAX, NULL},
};



EAPI void
evas_dump(Evas* evas)
{
	int width = 0, height = 0;

	FILE* pFile = null;
	int ret = 0;

	ret = system("rm -rf /tmp/layout");
	ret = system("mkdir /tmp/layout");
	if (ret)
	{
		return;
	}
	if(!evas)
	{
        PRINT("dump error : evas is null\n");
		return;
	}
    object_depth = 0;

	evas_output_size_get(evas,&width, &height);

	pFile = FOPEN("/tmp/layout/layout.html", "w");
	FPRINT(pFile, "<html><head><META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head>");
	FPRINT(pFile, "<body>\n");
	FPRINT(pFile, "<div id=\"information_container\" style=\"position:absolute; width:100%%; z-index:0;\"><div id=\"information\" style=\"font-size:9pt; position:absolute; left:850px; width:350px; z-index:0;\"></div></div>\n");
    FPRINT(pFile, "Evas:0x%x<br>\n", (unsigned int)evas);
	FPRINT(pFile, "<div style='position:absolute; left:0px; top:30px; width:%dpx; height:%dpx; '>", width, height);


	Evas_Layer *layer;
    PRINT("evas: 0x%x size(%d, %d)\n", (unsigned int)evas, width, height);
	EINA_INLIST_FOREACH(evas->layers, layer)
	  {
		PRINT("evas layer: 0x%x\n", (unsigned int)layer);
		 dump_layer(layer, pFile);
	  }

	FPRINT(pFile, "</div></div>\n");

    FPRINT(pFile, "<div id=\"tree_info\" style=\"font-size:10pt; position:absolute; top:0px; left:1220px; width:650px; z-index:%d; background-color:#BBDEDE; opacity:0.8;\">\n", object_depth+100);
    dump_tree(evas, pFile);
    FPRINT(pFile, "</div>");

    // script code
	FPRINT(pFile, "<script>\n");
    //object accessing handling
    FPRINT(pFile, "var spans = document.getElementsByTagName('span');\n");
    FPRINT(pFile, "var oldColor;\n\n");
    FPRINT(pFile, "for(var i = 0; i < spans.length; i++) {\n");
    FPRINT(pFile, " spans[i].addEventListener('mouseover', highlightSpanTarget, false);\n");
    FPRINT(pFile, " spans[i].addEventListener('mouseout', restoreSpanTarget, false);\n");
    FPRINT(pFile, " spans[i].addEventListener('click', informSpanTarget, false);\n");
    FPRINT(pFile, "}\n");
    FPRINT(pFile, "function highlightSpanTarget(event) {\n");
    FPRINT(pFile, "  var target = document.getElementById(this.textContent);\n");
    FPRINT(pFile, "  if(target == null) return;\n");
    FPRINT(pFile, "  oldColor = target.style.backgroundColor;\n");
    FPRINT(pFile, "  target.style.backgroundColor='yellow';\n");
    FPRINT(pFile, "}\n\n");

    FPRINT(pFile, "function restoreSpanTarget(event) {\n");
    FPRINT(pFile, "  var target = document.getElementById(this.textContent);\n");
    FPRINT(pFile, "  target.style.backgroundColor = oldColor;\n");
    FPRINT(pFile, "}\n\n");

    FPRINT(pFile, "function informSpanTarget(event) {\n");
    FPRINT(pFile, "  var target = document.getElementById(this.textContent);\n");
    FPRINT(pFile, "  if(target == null) return;\n");
    FPRINT(pFile, "  var tmp = getInfo(target)+information.innerHTML;\n");
    FPRINT(pFile, "  information.innerHTML = tmp;\n");
    FPRINT(pFile, "}\n\n");

    //object tree handling
    FPRINT(pFile, "var mousewheelevt=(/Firefox/i.test(navigator.userAgent))? 'DOMMouseScroll' : 'mousewheel' \n"); ////FF doesn't recognize mousewheel as of FF3.x

    FPRINT(pFile, "var tree = document.getElementById('tree_info');\n");
    FPRINT(pFile, "tree.addEventListener(mousewheelevt, function(e){\n");
    FPRINT(pFile, "    var evt=window.event || e;\n");
    FPRINT(pFile, "    var delta = evt.detail? evt.detail*(-20) : evt.wheelDelta;\n");
    FPRINT(pFile, "    var newpos = Number(tree.style.top.slice(0,-2)) + delta;\n");
    FPRINT(pFile, "    var tree_height = Number(tree.clientHeight);\n");
    FPRINT(pFile, "    if(newpos >= 0) newpos = 0;\n");
    FPRINT(pFile, "    else if(newpos <= -(tree_height-window.innerHeight)) newpos = -(tree_height-window.innerHeight);\n");
    FPRINT(pFile, "    this.style.top = newpos.toString()+'px';\n");
    FPRINT(pFile, "    console.log('wheel:'+ evt.detail +' (scroll:'+ delta+') pos:'+ this.style.top+'\\n');\n");
    FPRINT(pFile, "}, false);\n");
    FPRINT(pFile, "tree.addEventListener('mouseover',function(e){	document.body.style.overflow='hidden';},false);\n");
    FPRINT(pFile, "tree.addEventListener('mouseout',function(e){	document.body.style.overflow='';},false);\n");

    //object viewer handling
	FPRINT(pFile, "var divs = document.getElementsByTagName('div');\n");

	FPRINT(pFile, "for(var i = 0; i < divs.length; i++) {\n");
    FPRINT(pFile, "	if (divs[i].id != \"information_container\" && divs[i].id != \"information\"  && divs[i].id!=\"tree_info\"){\n");
    FPRINT(pFile, "		divs[i].addEventListener('mouseover', highlightThis, false);\n");
	FPRINT(pFile, "		divs[i].addEventListener('mouseout', restoreThis, false);\n");
	FPRINT(pFile, "		divs[i].addEventListener('click', informThis, false);\n");
	FPRINT(pFile, "		divs[i].addEventListener('dblclick', removeThis, false);\n");
	FPRINT(pFile, "	}");
	FPRINT(pFile, "}\n\n");

	FPRINT(pFile, "function highlightThis(event) {\n");
	FPRINT(pFile, "	oldColor = this.style.backgroundColor;\n");
	FPRINT(pFile, "	this.style.backgroundColor='yellow';\n");
	FPRINT(pFile, "	event.stopPropagation ? event.stopPropagation() : event.cancelBubble = true;\n");
	FPRINT(pFile, "}\n\n");

	FPRINT(pFile, "function restoreThis(event) {\n");
	FPRINT(pFile, "	this.style.backgroundColor = oldColor;\n");
	FPRINT(pFile, "	event.stopPropagation ? event.stopPropagation() : event.cancelBubble = true;\n");
	FPRINT(pFile, "}\n\n");

	FPRINT(pFile, "function informThis(event) {\n");
	FPRINT(pFile, " var tmp = getInfo(this)+information.innerHTML;\n");
	FPRINT(pFile, "	information.innerHTML = tmp;\n");
	FPRINT(pFile, "	event.stopPropagation ? event.stopPropagation() : event.cancelBubble = true;\n");
	FPRINT(pFile, "}\n\n");

	FPRINT(pFile, "function removeThis(event) {\n");
//	FPRINT(pFile, "	if (confirm(\"hide this layer?\")) this.style.display='none';\n");
	FPRINT(pFile, "	this.style.display='none';\n");
	FPRINT(pFile, "	event.stopPropagation ? event.stopPropagation() : event.cancelBubble = true;\n");
	FPRINT(pFile, "}\n\n");

    FPRINT(pFile, "function getInfo(o) {\n");
    FPRINT(pFile, "    if(information.hasChildNodes())\n");
    FPRINT(pFile, "    {\n");
    FPRINT(pFile, "      while(information.childNodes.length>=20)\n");
    FPRINT(pFile, "      {\n");
    FPRINT(pFile, "    information.removeChild(information.lastChild);\n");
    FPRINT(pFile, "      }\n");
    FPRINT(pFile, "    }\n");
    FPRINT(pFile, "    var obj = document.getElementById(this.id + \"_info\");\n");
    FPRINT(pFile, "    if (obj)\n");
    FPRINT(pFile, "    {\n");
    FPRINT(pFile, "       information.removeChild(obj);\n");
    FPRINT(pFile, "    }\n");
    FPRINT(pFile, "    var str = o.title;\n");
    FPRINT(pFile, "    var regexp = /(0x[a-z0-9]+$)/img;\n");
    FPRINT(pFile, "    var contents = o.style.backgroundImage ? \"<img src=\" + o.style.backgroundImage.substring(4, o.style.backgroundImage.length-1) + \" style='max-width:100%;max-height:100%'><br>\"\n");
    FPRINT(pFile, "              : \"<div style=\\\"width:100;height:30;border:1px dotted #999999; background-color:\" + oldColor + \"; opacity:\"+o.style.opacity+\";\\\"> </div>\";\n");
    FPRINT(pFile, "    str = \"<div id=\" + o.id + \"_info style='display:block;border:5px solid #EDEDED;background-color:#EDEDED;'>\" + contents + str + \"</div>\";\n");
    FPRINT(pFile, "    str = str.replace(/([^>\\r\\n]?)(\\r\\n|\\n\\r|\\r|\\n)/g, '$1<br>$2');\n");
    FPRINT(pFile, "    return str;\n");
    FPRINT(pFile, "}\n");

    FPRINT(pFile, "</script>\n");
	FPRINT(pFile, "</body>\n</html>\n");
	FCLOSE(pFile);
}

static void __attribute__((optimize("O0")))
dump_layer(Evas_Layer* layer, FILE* pFile)
{
/*
 * struct _Evas_Layer
{
   EINA_INLIST;

   short             layer;
   Evas_Object      *objects;

   Evas             *evas;

   void             *engine_data;
   int               usage;
   unsigned char     delete_me : 1;
};
*/
	Evas_Object* object;
    int depth = 0;
	EINA_INLIST_FOREACH(layer->objects, object)
	  {
         depth = dump_object(object, depth, pFile);
	  }

}

#if 0

struct _Evas_Object
{
   EINA_INLIST;

   DATA32                   magic;

   const char              *type;
   Evas_Layer              *layer;

   struct {
	  Evas_Map             *map;
	  Evas_Object          *clipper;
	  Evas_Object          *mask;
	  Evas_Object          *map_parent;
	  double                scale;
	  Evas_Coord_Rectangle  geometry;
	  Evas_Coord_Rectangle  bounding_box;
	  struct {
		 struct {
			Evas_Coord      x, y, w, h;
			unsigned char   r, g, b, a;
			Eina_Bool       visible : 1;
			Eina_Bool       dirty : 1;
		 } clip;
	  } cache;
	  short                 layer;
	  struct {
		 unsigned char      r, g, b, a;
	  } color;
	  Eina_Bool             usemap : 1;
	  Eina_Bool             valid_map : 1;
	  Eina_Bool             visible : 1;
	  Eina_Bool             have_clipees : 1;
	  Eina_Bool             anti_alias : 1;
	  Evas_Render_Op        render_op : 4;

	  Eina_Bool             valid_bounding_box : 1;
	  Eina_Bool             cached_surface : 1;
	  Eina_Bool             parent_cached_surface : 1;
   } cur, prev;

   struct {
	  void                 *surface; // surface holding map if needed
	  int                   surface_w, surface_h; // current surface w & h alloc
   } map;

   Evas_Map                   *cache_map;
   char                       *name;

   Evas_Intercept_Func        *interceptors;

   struct {
	  Eina_List               *elements;
   } data;

   Eina_List                  *grabs;

   Evas_Callbacks             *callbacks;

   struct {
	  Eina_List               *clipees;
	  Eina_List               *changes;
   } clip;

   const Evas_Object_Func     *func;

   void                       *object_data;

   struct {
	  Evas_Smart              *smart;
	  Evas_Object             *parent;
   } smart;

   struct {
	  Eina_List               *proxies;
	  void                    *surface;
	  int                      w,h;
	  Eina_Bool                redraw;
	  Eina_Bool                src_invisible : 1;
   } proxy;

#if 0 // filtering disabled
   Evas_Filter_Info           *filter;
#endif

   Evas_Size_Hints            *size_hints;

   RGBA_Map                   *spans;

   int                         last_mouse_down_counter;
   int                         last_mouse_up_counter;
   int                         mouse_grabbed;

   int                         last_event;
   Evas_Callback_Type          last_event_type;

   struct {
		int                      in_move, in_resize;
   } doing;

  /* ptr array + data blob holding all interfaces private data for
   * this object */
   void                      **interface_privates;

   unsigned int                ref;

   unsigned char               delete_me;

   unsigned char               recalculate_cycle;
   Eina_Clist                  calc_entry;

   Evas_Object_Pointer_Mode    pointer_mode : 2;

   Eina_Bool                   store : 1;
   Eina_Bool                   pass_events : 1;
   Eina_Bool                   freeze_events : 1;
   Eina_Bool                   repeat_events : 1;
   struct  {
	  Eina_Bool                pass_events : 1;
	  Eina_Bool                pass_events_valid : 1;
	  Eina_Bool                freeze_events : 1;
	  Eina_Bool                freeze_events_valid : 1;
	  Eina_Bool                src_invisible : 1;
	  Eina_Bool                src_invisible_valid : 1;
   } parent_cache;
   Eina_Bool                   restack : 1;
   Eina_Bool                   is_active : 1;
   Eina_Bool                   precise_is_inside : 1;
   Eina_Bool                   is_static_clip : 1;

   Eina_Bool                   render_pre : 1;
   Eina_Bool                   rect_del : 1;
   Eina_Bool                   mouse_in : 1;
   Eina_Bool                   pre_render_done : 1;
   Eina_Bool                   intercepted : 1;
   Eina_Bool                   focused : 1;
   Eina_Bool                   in_layer : 1;
   Eina_Bool                   no_propagate : 1;

   Eina_Bool                   changed : 1;
   Eina_Bool                   changed_move : 1;
   Eina_Bool                   changed_color : 1;
   Eina_Bool                   changed_map : 1;
   Eina_Bool                   changed_pchange : 1;
   Eina_Bool                   changed_src_visible : 1;
   Eina_Bool                   del_ref : 1;

   Eina_Bool                   is_frame : 1;
   Eina_Bool                   child_has_map : 1;
   Eina_Bool                   on_deletion : 1;
};

#endif



int get_object_type(Evas_Object* object)
{
	char* sz_type = evas_object_type_get(object);

	if(object->clip.clipees) // don't have to draw clipper
	{
		return O_CLIPPER;
	}

	else if(object->smart.smart)//smart object
	{
		return O_SMART;

	}
	else
	{
		if(!strcmp(sz_type, "image"))			return O_IMAGE;
		else if(!strcmp(sz_type, "text"))		return O_TEXT;
		else if(!strcmp(sz_type, "textblock"))	return O_TEXTBLOCK;
		else if(!strcmp(sz_type, "textgrid"))	return O_TEXTGRID;
		else if(!strcmp(sz_type, "rectangle"))	return O_RECTANGLE;
		else if(!strcmp(sz_type, "polygon"))		return O_POLYGON;
		else if(!strcmp(sz_type, "line"))		return O_LINE;
	}
	return O_NONE;
}

static void __attribute__((optimize("O0")))
_get_text_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText)
{
	char* font = null;
	int fontSize = 0;
	char* text = null;
	evas_object_text_font_get(object, &font, &fontSize);

	text = evas_object_text_text_get(object);


    snprintf(style, LOG_LEN_MAX, " opacity:%1.2f; font-size:%d; color:rgb(%d,%d,%d); background-color:#00000000; ",((float)alpha/(float)255), fontSize, red, green, blue);
	snprintf(title, LOG_LEN_MAX, "font=%s font-size=%d \ntext: %s", font, fontSize, text?text:"null");



	if(text)
	{
		strncpy(htmlText, text, LOG_LEN_MAX);
	}
}



static void  __attribute__((optimize("O0")))
_get_textblock_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText)
{
	char* font = "unknown";
    char* font_color = "#ffffff";
	int fontSize = 20;
	char* blockStyle = null;
    char tmpprop[LOG_LEN_MAX/2]={0,};

	blockStyle = evas_textblock_style_get(evas_object_textblock_style_get(object));

    // property
    _PropertyClass* pProperty = _get_textblock_property(blockStyle);

    if(pProperty)
      {
        char* value = pProperty->GetValue(pProperty,"font_size");
        if(value)
          {
            fontSize = (int)atof(value);
          }
        font_color = pProperty->GetValue(pProperty,"color");
//        if(font_color && strlen(font_color)==9)
//          {
//            font_color[8]=0;
//          }
        _PropertyClass_PrintOut( pProperty, tmpprop, sizeof(tmpprop)-1);
      }
    else
      {
        tmpprop[0]=0;
      }


    snprintf(style, LOG_LEN_MAX, " opacity:%1.2f; font-size:%d; color:%s; background-color:#00000000; ",((float)alpha/(float)255), fontSize, font_color);


	char* markup = evas_object_textblock_text_markup_get(object);

	char* text = evas_textblock_text_markup_to_utf8(object, markup);

    snprintf(title, LOG_LEN_MAX, "text: %s\ntext-style:\n%s\n",text, tmpprop);
	strncpy(htmlText, text, LOG_LEN_MAX);

	free(text);

    _PropertyClass_Delete(pProperty);
    memset(tmpprop,0,sizeof(tmpprop));
}

static void  __attribute__((optimize("O0")))
_get_textgrid_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText)
{
#if 0
	char* font = null;
	int fontSize = 0;
	font = evas_object_textgrid_font_source_get(object);

	snprintf(style, LOG_LEN_MAX, " opacity:%1.2f; font-size:%d; color:rgb(%d,%d,%d); background-color:#00000000; ",((float)alpha/(float)255), fontSize, red, green, blue);
	snprintf(title, "font=%s font-size=%d", font, fontSize);
#endif
    PRINT("need to implement : _get_textgrid_desc \n");
}

static void  __attribute__((optimize("O0")))
_get_image_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText)
{
	char filename[LOG_LEN_MAX/4] = {0, };
	char savePath[LOG_LEN_MAX/2] = {0, };
	int imgW = 1,imgH = 1;
	int fillX = 0, fillY = 0, fillW = 0, fillH = 0;

	Evas_Object* source = evas_object_image_source_get(object);

	evas_object_image_size_get(source, &imgW, &imgH);
	evas_object_image_fill_get(object, &fillX , &fillY , &fillW , &fillH );

	if (sz_name && sz_name[0] != 0)
	{
		snprintf(filename, sizeof(filename), "e_img_%s_%08x.png", sz_name, (unsigned int)object);
	}
	else
	{
		snprintf(filename, sizeof(filename), "e_img_%08x.png", (unsigned int)object);
	}

	// save the image file
	strncpy(savePath, "/tmp/layout/", 12);
	strncat(savePath, filename, strlen(filename));
	evas_object_image_save(object, savePath, NULL, "quality=100 compress=9");

	snprintf(style, LOG_LEN_MAX, "background-image:url('%s'); background-size:%dpx %dpx; ", filename, fillW, fillH);
	snprintf(title, LOG_LEN_MAX, "%s\nImage: w=%d h=%d Filled Bounds: x=%d y=%d w=%d h=%d\n",
			 filename,
			 w, h,
			 fillX, fillY, fillW, fillH);
}

static void  __attribute__((optimize("O0")))
_get_default_desc(Evas_Object* object, const char* sz_type, const char* sz_name,int x, int y, int w, int h, int red, int green, int blue, int alpha, char* style, char* title,char* htmlText)
{
	if (1)//alpha)
	{
//			snprintf(style1, LOG_LEN_MAX, "color:#%08x; ", rgb);
		snprintf(style, LOG_LEN_MAX, "opacity:%1.2f; background-color:rgb(%d,%d,%d); ", ((float)alpha/(float)255), red, green, blue);
	}
	else
	{
		snprintf(style, LOG_LEN_MAX, "pointer-events: none; ");
	}
	title[0]=' ';
}





static int __attribute__((optimize("O0")))
dump_object(Evas_Object* object, int depth, FILE* pFile)
{
    int x = 0, y = 0, w = 0, h = 0;
    int visible = 0;
//	int evasVisible = 0;
//	int depth = 0;
    char style1[LOG_LEN_MAX+1] = {0, };
    char title1[LOG_LEN_MAX+1] = {0, };
    char style[LOG_LEN_MAX+1] = {0, };
    char title[LOG_LEN_MAX+1] = {0, };
    char htmlText[LOG_LEN_MAX+1] = {0, };
    char logMessage[LOG_LEN_MAX+1] = {0, };
    char endTag[256] = {0, };
    char* sz_type = NULL;
    char* sz_name = NULL;

    objectType objType = O_NONE;

    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    int borderSize = 1;
    unsigned int rgb = 0;
    Evas_Object* pDepth = null;
    if(object->delete_me)
      return depth;

    evas_object_geometry_get(object, &x, &y, &w, &h);
    visible = evas_object_visible_get(object);
//	evasVisible = evas_visible_get(evas_object_evas_get(object));
    evas_object_color_get(object, &red, &green, &blue, &alpha);

//	depth = -1;
//	pDepth = object;
//	while (pDepth)
//	{
//		depth++;
//		pDepth = pDepth->smart.parent;
//	}


    objType = get_object_type(object);

    if(!visible)
        return depth;
    sz_type = evas_object_type_get(object);
    sz_name = evas_object_name_get(object);

    depth += 1;

    if(objType == O_NONE)
        return;
    if(objType == O_SMART)
    {
        //dump child
        Eina_Inlist* contained = evas_object_smart_members_get_direct(object);

        Evas_Object* child = NULL;

        EINA_INLIST_FOREACH(contained, child)
        {
             depth = dump_object(child, depth, pFile);
        }
        return depth;
    }
    else if(objParser[objType].getDesc)
    {
        objParser[objType].getDesc(object, sz_type, sz_name, x, y, w, h, red, green, blue, alpha, style1, title1, htmlText);
    }
    else
    {
        return depth;
    }

    if(object->smart.parent)
      {
        snprintf(title, LOG_LEN_MAX, "[0x%x] type=%s name=%s\n parent(%s:%s,%#x) \nvisible=%s Color:rgba(%d,%d,%d,%d) \nEvas Absolute Bounds: x=%d y=%d w=%d h=%d\n %s\n",
                  (unsigned int)object, sz_type, (sz_name ? sz_name:"null"), evas_object_type_get(object->smart.parent),evas_object_name_get(object->smart.parent),(unsigned int)object->smart.parent,
                  (visible ? "show" : "hide"),
                 red, green, blue, alpha, x, y, w, h,
                 title1 );
      }
    else
      {
        snprintf(title, LOG_LEN_MAX, "[0x%x] type=%s name=%s\n parent(null) \nvisible=%s Color:rgba(%d,%d,%d,%d) \nEvas Absolute Bounds: x=%d y=%d w=%d h=%d\n %s\n",
                  (unsigned int)object, sz_type, (sz_name ? sz_name:"null"),
                  (visible ? "show" : "hide"),
                 red, green, blue, alpha, x, y, w, h,
                 title1 );
      }

//	snprintf(style, LOG_LEN_MAX, "%sopacity:%d; position:absolute; left:%dpx; top:%dpx; width:%dpx; height:%dpx; border:1px dotted #aaaaaa; z-index:%d; display:%s;%s",
//			 style1, alpha, x, y, w, h, depth, visible ? "block" : "none", " overflow:hidden;");

    snprintf(style, LOG_LEN_MAX, "%s  position:absolute; left:%dpx; top:%dpx; width:%dpx; height:%dpx; border:1px dotted #aaaaaa; z-index:%d; display:%s;%s",
             style1, x, y, w, h, object_depth, visible ? "block" : "none", " overflow:hidden;");
    object_depth+=2;

    FPRINT(pFile, "%s%*s<div id=\"%#x\" ", endTag, depth, " ", (unsigned int)object);
    FPRINT(pFile, "style=\"%s\" ",style);
    FPRINT(pFile, "title=\"%s\">\n",title);

//	snprintf(logMessage, LOG_LEN_MAX, "%s%*s<div id=\"%#x\" style=\"%s\" title=\"%s\">\n", endTag, depth, " ", (unsigned int)object, style, title);
//	FPUTS(pFile,logMessage);

    if(htmlText[0] != '\0')
    {
        FPRINT(pFile,"%s", htmlText);
    }


    FPUTS(pFile,"</div>\n");

    return depth;

}



/*
 * object_tree_info
*/

static int __is_widget(Evas_Object* obj)
{
  return evas_object_smart_type_check_ptr(obj, "elm_widget");
}

static Eina_Bool
__evas_object_is_visible_get(Evas_Object *obj)
{
   int r, g, b, a;
   Evas_Coord x, y, w, h;
   Evas_Object *clip;
   int vis = 0;

   evas_object_color_get(obj, &r, &g, &b, &a);
   evas_object_geometry_get(obj, &x, &y, &w, &h);

   if (evas_object_visible_get(obj)) vis = 1;
   clip = evas_object_clip_get(obj);
   if (clip) evas_object_color_get(obj, &r, &g, &b, &a);
   if (clip && !evas_object_visible_get(clip)) vis = 0;

   if (clip && a == 0) vis = 0;

   return vis;
}

static Eina_Bool
__evas_object_type_match(const Evas_Object *obj, const char *type)
{
   int ret;
   ret = strcmp(evas_object_type_get(obj), type);

   if (!ret) return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
__evas_object_is_swallow_rect(const Evas_Object *obj)
{
   int r, g, b, a;

   evas_object_color_get(obj, &r, &g, &b, &a);
   if (!r && !g && !b && !a
       && evas_object_pointer_mode_get(obj) == EVAS_OBJECT_POINTER_MODE_NOGRAB
       && evas_object_pass_events_get(obj))
     return EINA_TRUE;

   return EINA_FALSE;
}

static void
_obj_tree_items(Evas_Object *obj, int lvl, FILE* pFile)
{
   Eina_List *children, *l;
   Evas_Object *child, *smart_parent_obj;
   Evas_Coord x, y, w, h;
   Eina_Bool is_clip = EINA_FALSE;
   int i, r, g, b, a;
   static int num = 0;
   const char *ret;
   double val;
   const char *file, *key, *group;
//   Edje_Info *edje_info;

   // visible check
   if (!__evas_object_is_visible_get(obj)) return;

   // viewport check
   evas_object_geometry_get(obj, &x, &y, &w, &h);

   // clipper check
   if (evas_object_clipees_get(obj)) is_clip = EINA_TRUE;
   if (is_clip) goto next;

   char visible_ch = '-';
   if (__evas_object_is_visible_get(obj)) visible_ch = '+';

   FPRINT(pFile,"<li><span style=\"color:red\">0x%x</span> [%4d %4d %4d %4d  %c]", obj, x, y, w, h, visible_ch);

   num++;

   for (i = 0; i < lvl; i++)
     {
      FPRINT(pFile, "-");
     }

   // evas object type check
   if (__evas_object_is_swallow_rect(obj)) FPRINT(pFile,"[%d] swallow ", lvl);
   else if (__evas_object_type_match(obj, "rectangle"))
     {
        evas_object_color_get(obj, &r, &g, &b, &a);
        FPRINT(pFile,"[%d] rect [%02X %02X %02X %02X] ", lvl,  r, g, b, a);
     }
   else FPRINT(pFile,"[%d] %s ", lvl, evas_object_type_get(obj));

   smart_parent_obj = evas_object_smart_parent_get(obj);

   // image info save
//   if (!strcmp(evas_object_type_get(obj), "elm_icon") ||
//       !strcmp(evas_object_type_get(obj), "elm_image"))
//     {
//        elm_image_file_get(obj, &file, &key);
//        evas_object_data_set(obj, "image_name", file);
//     }

   // image name check
   if (smart_parent_obj && __evas_object_type_match(obj, "image")
       && (__evas_object_type_match(smart_parent_obj, "elm_icon")
           || __evas_object_type_match(smart_parent_obj, "elm_image")))
     {
//        if (ret = evas_object_data_get(smart_parent_obj, "image_name"))
//          {
//             _extract_edje_file_name(util_mgr, ret);
//             evas_object_data_del(smart_parent_obj, "edje_image_name");
//          }
     }

   // edje info save
   if(__evas_object_type_match(obj, "edje"))
     {
#if 0
        edje_object_file_get(obj, &file, &group);
        if (group) FPRINT(pFile,"[%s] ", group);

        _extract_edje_file_name(util_mgr, file);
        _edje_file_info_save(util_mgr, obj);
#endif
     }

   // edje info check
   if( !is_clip && smart_parent_obj
       && !__is_widget(obj) && __evas_object_type_match(smart_parent_obj, "edje"))
     {

#if 0
       const Evas_Object *pobj;
       Eina_List *parts = edje_edit_parts_list_get(smart_parent_obj);
       Eina_List *l;
       double val;
       EINA_LIST_FOREACH(parts, l, pname)
         {
            if ((pobj = edje_object_part_object_get(obj, pname)))
              {

                edje_info = malloc(sizeof(Edje_Info));
                edje_info->obj = pobj;
                strcpy(edje_info->part_name, pname);
                ret = edje_object_part_state_get(obj, edje_info->part_name, &val);

                if(edje_info->obj == obj)
                  {
                     if (edje_info->color_class)
                       FPRINT(pFile,"[%s] ", edje_info->color_class);

                     ret = edje_object_part_state_get(evas_object_smart_parent_get(obj), edje_info->part_name, &val);
                     FPRINT(pFile,"[%s : \"%s\" %2.1f]  ", edje_info->part_name , ret, val);

                     if (edje_info->image_name)
                       FPRINT(pFile,"[%s] ", edje_info->image_name);
                     break;
                  }

                 edje_info = malloc(sizeof(Edje_Info));
                 edje_info->obj = pobj;
                 strcpy(edje_info->part_name, pname);
                 ret = edje_object_part_state_get(obj, edje_info->part_name, &val);

                 if (ret)
                   {
                      edje_info->color_class = edje_edit_state_color_class_get(ed, edje_info->part_name , ret, val);
                      edje_info->image_name = edje_edit_state_image_get(ed, edje_info->part_name , ret, val);
                   }

                 util_mgr->edje_part_name_list = eina_list_append(util_mgr->edje_part_name_list, edje_info);

              }
         }

        EINA_LIST_FOREACH(util_mgr->edje_part_name_list, l, edje_info)
          {
             if(edje_info->obj == obj)
               {
                  if (edje_info->color_class)
                    FPRINT(pFile,"[%s] ", edje_info->color_class);

                  ret = edje_object_part_state_get(evas_object_smart_parent_get(obj), edje_info->part_name, &val);
                  FPRINT(pFile,"[%s : \"%s\" %2.1f]  ", edje_info->part_name , ret, val);

                  if (edje_info->image_name)
                    FPRINT(pFile,"[%s] ", edje_info->image_name);
                  break;
               }
          }
#endif
     }


   if (!strcmp(evas_object_type_get(obj), "text"))
     {
      char* text = evas_object_text_text_get(obj);

        FPRINT(pFile,"<p style=\"text-indent: 5em;\">content: [%s]</p>", text?text:"null");
     }
   else if (!strcmp(evas_object_type_get(obj), "textblock"))
     {
        ret = evas_object_textblock_text_markup_get(obj);
        char* text = evas_textblock_text_markup_to_utf8(obj, ret);
        FPRINT(pFile,"<p style=\"text-indent: 15em;\">content: [%s]</p>", text?text:"null");
        free(text);
     }
    FPRINT(pFile,"</li>\n");
//    FPRINT(pFile,"<br>\n");
//   FPRINT(pFile,"\n\033[37;1m");
next:
   children = evas_object_smart_members_get(obj);
   EINA_LIST_FREE(children, child)
      _obj_tree_items(child, lvl + 1, pFile);
}


static void
dump_tree(Evas* evas, FILE* pFile)
{
   Evas_Coord x, y, w, h;


  evas_output_viewport_get(evas, &x, &y, &w, &h);
  FPRINT(pFile,"<pre>\n");

   FPRINT(pFile,"---------------------------------  Object Tree Info  ----------------------------------------- <br>\n");
   FPRINT(pFile, "Evas : %p [%d %d %d %d] \n\n", evas, x, y, w, h);
   FPRINT(pFile,"No : Address    [   x    y    w    h  visible] <br>\n");
   FPRINT(pFile,"---------------------------------  Object Tree Info  ----------------------------------------- <br>\n");

   Evas_Layer *layer;
   EINA_INLIST_FOREACH(evas->layers, layer)
     {
       PRINT("evas layer: 0x%x\n", (unsigned int)layer);
       Evas_Object* object;

       FPRINT(pFile,"[evas layer: 0x%x]\n", (unsigned int)layer);
       FPRINT(pFile,"<ol>\n");
       EINA_INLIST_FOREACH(layer->objects, object)
       {
          _obj_tree_items( object, 0, pFile);
       }
       FPRINT(pFile,"</ol>\n");
     }
   FPRINT(pFile,"\n-----------------------------------------------------------------------------------------------<br>\n");
   FPRINT(pFile,"</pre>\n");
}
