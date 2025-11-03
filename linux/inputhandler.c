// Tronche's docs
/* typedef struct {
   int type;
   unsigned long serial;	 # of last request processed by server 
Bool send_event;	 # true if this came from a SendEvent request 
Display *display;	 # Display the event was read from 
Window window;
} XAnyEvent;*/
void inputHandler(XEvent event, char * keys, KeySym * keysym)
{
    switch(event.type)
    {
        case MapNotify:
            break;
        case FocusIn:
            bActiveWindow = True;
            break;
        case FocusOut:
            bActiveWindow = False;
            break;
        case ConfigureNotify:
            resize(event.xconfigure.width, event.xconfigure.height);
            break;
        case KeyPress:
            keysym = XkbKeycodeToKeysym(event.xany.display, event.xkey.keycode, 0, 0);
            switch(keysym)
            {
                case XK_Escape:
                    bDone = True;
                    break;
                default:
                    break;
            }

            XLookupString(&event.xkey, keys, sizeof(keys), NULL, NULL);
            switch(keys[0])
            {
                case 'F':
                case 'f':
                    printf("KeyPressed F or f\n");
                    if(bFullscreen == false)
                    { 
                        bFullscreen = true; 
                        toggleFullScreen();
                    }
                    else 
                    { 
                        bFullscreen = false; 
                        toggleFullScreen();
                    }

                    break;
                default:
                    break;
            }

            break;
        case ButtonPress:
            switch(event.xbutton.button)
            {
                case 1: // LEFT
                    break;
                case 2: // MID
                    break;
                case 3: // RIGHT
                    break;
            }
            break;
        case 33:
            bDone = True;
            break;
        default :
            break;
    }
}
