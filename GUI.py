import gi
import serial
import time
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gio, GObject

class MainWindow(Gtk.Window):

    #might need some boolean variables later to notice the change of parameters during pause

    def __init__(self):
        #the serial communication part, where to put ser.close()?
        self.ser = serial.Serial(timeout=0.1)
        self.ser.baurate = 9600
        self.ser.port = "/dev/ttyACM0"
        print(self.ser)
        self.ser.open()
        print(self.ser.is_open)

        # setting variables
        self.control = 0  # 0:stop,1:run/continue,2:cancel previous task and run new task
        self.type = 1  # 1:10ml, 2:20ml, 5:50ml...
        self.mode = 0  # 0:infuse only, 1:refill only, 2: refill after infuse
        self.speed = 0  # target speed, in unit of uL/s
        self.volume = 0  # target volume, in unit of uL
        self.speed_1 = 0
        self.speed_2 = 0
        self.volume_1 = 0
        self.volume_2 = 0
        self.total_time = 1.0
        self.remain_time = 0.0
        self.finished_volume = 0.0
        self.dist2vol = 0.36 #for 30mL syringe

        Gtk.Window.__init__(self, title="My syringe pump")
        self.set_border_width(10)
        #self.set_default_size(800, 480)

        #grid
        grid = Gtk.Grid()
        self.add(grid)

        #header bar
        hb = Gtk.HeaderBar()
        hb.set_show_close_button(True)
        hb.props.title = "Syringe Pump"
        self.set_titlebar(hb)

        #top horizontal box
        hbox_top = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,spacing=50)
        grid.add(hbox_top)

        # Setting box
        set_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL,spacing=10)
        hbox_top.pack_start(set_box, False, True, 0)
        set_label = Gtk.Label("Settings")
        set_box.pack_start(set_label, False, True, 0)

        # Syringe select button
        ssb= Gtk.Button("Syringe type")
        ssb.connect("clicked", self.on_select_syringe)
        set_box.pack_start(ssb, False, True, 0)

        #volumn button
        vb = Gtk.Button("Volume")
        vb.connect("clicked", self.on_set_volume)
        set_box.pack_start(vb, False, True, 0)

        #speed button
        sb = Gtk.Button("Speed")
        sb.connect("clicked", self.on_set_speed)
        set_box.pack_start(sb, False, True, 0)

        #mode button
        mb = Gtk.Button("Mode")
        mb.connect("clicked", self.on_set_mode)
        set_box.pack_start(mb, False, True, 0)

        #set box border
        set_box_border = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
        hbox_top.pack_start(set_box_border,False,True,0)
        #hbox_top.set_center_widget(set_box_border)

        #info box
        info_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        hbox_top.pack_start(info_box,False,True,0)

        #task info label
        self.task_label = Gtk.Label("Waiting for giving a task...")
        info_box.pack_start(self.task_label,False,True,0)
        #info_box.set_center_widget(info_label)

        #progress bar
        self.pb = Gtk.ProgressBar(fraction=0.5,show_text=True)
        self.pb.set_text("Task Progress")
        info_box.pack_start(self.pb,False,True,0)

        #position label: would become the volume label later
        self.pb_label = Gtk.Label("    ")
        info_box.pack_start(self.pb_label, False, True, 0)

        #task remain time label
        self.time_label = Gtk.Label("Task Remain Time : ")
        info_box.pack_start(self.time_label,False,True,0)

        #top horizontal box boarder
        hbox_border = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        grid.attach_next_to(hbox_border,hbox_top,Gtk.PositionType.BOTTOM,1,1)

        #bottom horizontal box
        hbox_bottom = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        grid.attach_next_to(hbox_bottom,hbox_border,Gtk.PositionType.BOTTOM,1,1)

        #begin/continue task button
        bb = Gtk.Button("Start")
        bb.connect("clicked", self.on_start)
        hbox_bottom.pack_start(bb, False, True, 0)

        #stop task button
        stb = Gtk.Button("Stop")
        stb.connect("clicked", self.on_stop)
        hbox_bottom.pack_end(stb, False, True, 0)

    def prepare_task(self):
        if self.remain_time>0:
            #have a pop up to ask continue or restart
            pre_dialog = PrepareDialog(self)
            response = pre_dialog.run()
            if response == Gtk.ResponseType.OK:
                self.volume = self.volume - self.finished_volume
            if response == Gtk.ResponseType.CANCEL:
                self.control = 2
            pre_dialog.destroy()
        self.volume_1 = int(self.volume / 250)
        self.volume_2 = int(self.volume - self.volume_1 * 250)
        self.speed_1 = int(self.speed / 250)
        self.speed_2 = int(self.speed - self.speed_1 * 250)
        self.total_time = self.volume / self.speed
        self.remain_time = self.total_time
        #print(self.remain_time)

    def on_start(self,widget):
        if self.control== 0:
            #dc_dialog = DoubleCheckDialog(self)
            #might have a double check window to confirm the setting
            self.control = 1
            print("Start task")
            #command prepare, need a function for it
            self.prepare_task()
            #update info label
            if self.mode ==0:
                task_info = "Infuse {} uL at {} uL/s"
                self.task_label.set_text(task_info.format(self.volume, self.speed))
            if self.mode ==1:
                task_info = "Refill {} uL at {} uL/s"
                self.task_label.set_text(task_info.format(self.volume, self.speed))
            #send command
            p = [self.control, self.type, self.mode, self.volume_1, self.volume_2, self.speed_1, self.speed_2]
            print(p)
            self.ser.write(p)

        else:
            print("Please Stop the task first")

    def on_stop(self,widget):
        if self.control==1:
            self.control = 0
            print("Stop task")
            #send command
            p = [self.control, self.type, self.mode, self.volume_1, self.volume_2, self.speed_1, self.speed_2]
            self.ser.write(p)
            self.get_finished_volume()
        else:
            print("The task has stopped")

    def on_set_speed(self,widget):
        sb_dialog = SpeedDialog(self)
        response = sb_dialog.run()
        # response handling
        if response == Gtk.ResponseType.OK:
            #the unit we use for speed is uL/s
            self.speed = float(sb_dialog.speed_entry.get_text())
            if sb_dialog.selected_unit == 0:
                print("You set the speed to ", self.speed, "uL/s")
            elif sb_dialog.selected_unit == 1:
                print("You set the speed to ", self.speed, "mL/s")
                self.speed = self.speed*1000
            elif sb_dialog.selected_unit == 2:
                print("You set the speed to ", self.speed, "uL/min")
                self.speed = self.speed*60
            elif sb_dialog.selected_unit == 3:
                print("You set the speed to ", self.speed, "mL/min")
                self.speed = self.speed*1000*60
        elif response == Gtk.ResponseType.CANCEL:
            print("You cancel the speed Setting")
        sb_dialog.destroy()

    def on_set_volume(self,widget):
        vb_dialog = VolumeDialog(self)
        response = vb_dialog.run()
        # response handling, need to convert the entry volume string to number
        if response == Gtk.ResponseType.OK:
            if vb_dialog.selected_unit == 0:
                self.volume = float(vb_dialog.volume_entry.get_text())
                print("The target volume is",self.volume,"uL")
            elif vb_dialog.selected_unit == 1:
                #times 1000 as the unit of volume is uL
                self.volume = float(vb_dialog.volume_entry.get_text())*1000
                print("The target volume is", self.volume/1000, "mL")
        elif response == Gtk.ResponseType.CANCEL:
            print("You cancel the volume Setting")
        vb_dialog.destroy()

    def on_set_mode(self,widget):
        mb_dialog = ModeDialog(self)
        response = mb_dialog.run()
        #response handling
        if response == Gtk.ResponseType.OK:
            self.mode = mb_dialog.selected_mode
            print("You set the Mode to", self.mode)

        elif response == Gtk.ResponseType.CANCEL:
            print("You cancel the Mode Setting, remain Mode",self.mode)
        mb_dialog.destroy()

    def on_select_syringe(self,widget):
        ssb_dialog = SyringeDialog(self)
        response = ssb_dialog.run()
        # response handling
        if response == Gtk.ResponseType.OK:
            self.type = ssb_dialog.selected_type
            print("You select the ",self.type*10, "ml syringe")
        elif response == Gtk.ResponseType.CANCEL:
            print("You cancel the Syringe type Selection")
        ssb_dialog.destroy()

    #might also set a reset button
    def reset(self):
        #reset all the setting parameters
        self.control = 0  # 0:stop,1:run
        self.type = 1  # 1:10ml, 2:20ml, 5:50ml...
        self.mode = 0  # 0:infuse only, 1:refill only, 2: refill after infuse
        self.speed = 0  # target speed, in unit of uL/s
        self.volume = 0  # target volume, in unit of uL

    #need some function to handle the message through the serial
    def get_finished_volume(self):
        #  putting our datetime into a var and setting our label to the result.
        #  we need to return "True" to ensure the timer continues to run, otherwise it will only run once.
        pfv=0.0
        while 1:
            good = self.ser.read()
            #print(good)
            #get info about the finished distance in mm
            if good == b'A':
                num = int(self.ser.read())
                dist = int(self.ser.read(num))
                cfv = int(dist * self.dist2vol * 1000)
                if pfv == cfv:
                   self.finished_volume=cfv
                   self.pb_label.set_text("Have Finished: {}uL".format(self.finished_volume))
                   print(self.finished_volume)
                   break
                pfv=cfv
             #get info about the motor status, might check abnormal stopping behavior(get stuck?)
            #elif good == b'B':
                #self.control = 0

    #just for clear the buffer or we could change the position sending mechanism
    def clear_buffer(self):
        good = self.ser.read()
        #print(good)
        # get info about the finished distance in mm
        if good == b'A':
            num = int(self.ser.read())
            dist = int(self.ser.read(num))
            #self.pb_label.set_text("Have Finished: {}uL".format(dist))
            #print(dist)
        return True

    def display_time(self):
        #  putting our datetime into a var and setting our label to the result.
        #  we need to return "True" to ensure the timer continues to run, otherwise it will only run once.
        #  update the remain time and progress bar
        #  when the time runs out, update the control status to stop for next operation
        if self.control == 1:
            if self.remain_time>0:
                self.time_label.set_text("Task Remain Time: {}s ".format(self.remain_time))
                self.remain_time=int(self.remain_time-1)
            else:
                self.time_label.set_text("Task Remain Time: 0s ")
                self.control = 0
                self.get_finished_volume()
                self.remain_time = 0
                self.task_label.set_text("Finish Task")
                self.pb_label.set_text("Finish target volume")
        self.pb.set_fraction((self.total_time-self.remain_time)/self.total_time)
        return True

    # Initialize Timer
    def startclocktimer(self):
        #  this takes 2 args: (how often to update in millisec, the method to run)
        GObject.timeout_add(1000, self.clear_buffer)
        GObject.timeout_add(1000, self.display_time)

class ModeDialog(Gtk.Dialog):

    def __init__(self,parent):
        Gtk.Dialog.__init__(self, "Setting the Mode", parent, 0,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))
        self.set_default_size(400, 200)
        box = self.get_content_area()

        self.selected_mode = 0

        button1 = Gtk.RadioButton.new_with_label_from_widget(None, "Infuse Only(Default)")
        button1.connect("toggled", self.on_button_toggled, 0)
        box.pack_start(button1, False, False, 0)

        button2 = Gtk.RadioButton.new_with_label_from_widget(button1, "Refill Only")
        button2.connect("toggled", self.on_button_toggled, 1)
        box.pack_start(button2, False, False, 0)

        button3 = Gtk.RadioButton.new_with_label_from_widget(button1,"Infuse&Refill(not available)")
        button3.connect("toggled", self.on_button_toggled, 2)
        box.pack_start(button3, False, False, 0)
        self.show_all()

    #will update the state to appropriate variable
    def on_button_toggled(self, button, mode):
        if button.get_active():
            self.selected_mode = mode
            print("Mode ", self.selected_mode)

class SyringeDialog(Gtk.Dialog):
    def __init__(self, parent):
        Gtk.Dialog.__init__(self, "Select the Syringe type", parent, 0,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))

        self.selected_type = 1 #default syringe type

        self.set_default_size(400, 200)
        box = self.get_content_area()

        button1 = Gtk.RadioButton.new_with_label_from_widget(None, "10mL(Default)")
        button1.connect("toggled", self.on_button_toggled, 1)
        box.pack_start(button1, False, False, 0)

        button2 = Gtk.RadioButton.new_with_label_from_widget(button1, "30mL")
        button2.connect("toggled", self.on_button_toggled, 3)
        box.pack_start(button2, False, False, 0)

        button3 = Gtk.RadioButton.new_with_label_from_widget(button1, "50mL")
        button3.connect("toggled", self.on_button_toggled, 5)
        box.pack_start(button3, False, False, 0)
        self.show_all()

    def on_button_toggled(self, button, type):
        if button.get_active():
            self.selected_type = type
            print("Syringe Type ", type)

class SpeedDialog(Gtk.Dialog):
    def __init__(self, parent):
        Gtk.Dialog.__init__(self, "Set the speed", parent, 0,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))

        self.set_default_size(400, 200)
        self.selected_unit = 0# 0: uL/s, 1:mL/s, 2: uL/min, 3:mL/min

        box = self.get_content_area()

        #speed value entry
        self.speed_entry = Gtk.Entry()
        self.speed_entry.set_text("Type the target speed here and select the unit")
        box.pack_start(self.speed_entry,False,False,0)

        #speed unit selection
        button1 = Gtk.RadioButton.new_with_label_from_widget(None, "uL/s(Default)")
        button1.connect("toggled", self.on_button_toggled, 0)
        box.pack_start(button1, False, False, 0)

        button2 = Gtk.RadioButton.new_with_label_from_widget(button1, "mL/s")
        button2.connect("toggled", self.on_button_toggled, 1)
        box.pack_start(button2, False, False, 0)

        button3 = Gtk.RadioButton.new_with_label_from_widget(button1, "uL/min")
        button3.connect("toggled", self.on_button_toggled, 2)
        box.pack_start(button3, False, False, 0)

        button4 = Gtk.RadioButton.new_with_label_from_widget(button1, "mL/min")
        button4.connect("toggled", self.on_button_toggled, 3)
        box.pack_start(button4, False, False, 0)

        self.show_all()

    def on_button_toggled(self, button, unit):
        if button.get_active():
            self.selected_unit = unit

class VolumeDialog(Gtk.Dialog):
    def __init__(self, parent):
        Gtk.Dialog.__init__(self, "Set the target volume", parent, 0,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))

        self.selected_unit = 0 # 0:uL, 1:mL
        self.set_default_size(400, 200)
        box = self.get_content_area()

        # volume value entry
        self.volume_entry = Gtk.Entry()
        self.volume_entry.set_text("Type the target volume here and select the unit")
        box.pack_start(self.volume_entry,False,False,0)

        # volume unit selection
        button1 = Gtk.RadioButton.new_with_label_from_widget(None, "uL(Default)")
        button1.connect("toggled", self.on_button_toggled, 0)
        box.pack_start(button1, False, False, 0)

        button2 = Gtk.RadioButton.new_with_label_from_widget(button1, "mL")
        button2.connect("toggled", self.on_button_toggled, 1)
        box.pack_start(button2, False, False, 0)

        self.show_all()

    def on_button_toggled(self,button,unit):
        if button.get_active():
            self.selected_unit = unit

class PrepareDialog(Gtk.Dialog):
    def __init__(self,parent):
        Gtk.Dialog.__init__(self, "Choose to continue or not", parent, 0,
                            (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                             Gtk.STOCK_OK, Gtk.ResponseType.OK))

        self.set_default_size(400, 200)
        box = self.get_content_area()
        pro_label = Gtk.Label("Would you like to continue the remain volume from the last task?")
        box.pack_start(pro_label, False, False, 0)
        self.show_all()

#run the main window
win = MainWindow()
win.connect("destroy", Gtk.main_quit)
win.show_all()
win.startclocktimer()
Gtk.main()
