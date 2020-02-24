extends RigidBody

# Called when the node enters the scene tree for the first time.
func _ready():
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

var FORCE = 0.3

# Called every frame. 'delta' is the elapsed time since the previous frame.
# warning-ignore:unused_argument
func _process(delta):
	var dir = Vector3(0, 0, 0)
	if Input.is_action_pressed("ui_right"):
		dir.x += 1
	if Input.is_action_pressed("ui_left"):
		dir.x -= 1
	if Input.is_action_pressed("ui_down"):
		dir.z += 1
	if Input.is_action_pressed("ui_up"):
		dir.z -= 1
	if Input.is_action_pressed("ui_select"):
		dir.y += 1
	if Input.is_action_just_pressed("ui_accept"):
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		else:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	if Input.is_action_just_pressed("ui_focus_next"):
			var idx = AudioServer.get_bus_index("Record")
			var effect = AudioServer.get_bus_effect(idx, 0)
			effect.set_recording_active(!effect.is_recording_active())

	dir = dir.rotated(Vector3(0, 1, 0), rotation.y)
	apply_impulse(Vector3(0, 0, 0), dir * FORCE)
	if translation.y < -100:
		translation = Vector3(0, 0, 0)
		rotation = Vector3(0, 0, 0)

var MOUSE_SENSITIVITY = 0.1

func _input(event):
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		apply_torque_impulse(Vector3(0, -0.03 * event.relative.x * MOUSE_SENSITIVITY, 0))
		var tmpRotation = $Camera.rotation
		tmpRotation.x -= deg2rad(event.relative.y * MOUSE_SENSITIVITY)
		tmpRotation.x = clamp(tmpRotation.x, -PI / 2, +PI / 2)
		$Camera.rotation = tmpRotation
