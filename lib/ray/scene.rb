module Ray
  class Scene
    include Ray::Helper

    class << self
      # Registers a scene to a game object, used for subclasses.
      def bind(game)
        game.scene(scene_name, self)
      end

      # @overload scene_name
      #   @return [Symbol] the name of the scene
      # @overload scene_name(value)
      #   Sets the name of the scene
      def scene_name(val = nil)
        @scene_name = val || @scene_name
      end
    end

    scene_name :scene

    # Creates a new scene. block will be instance evaluated when
    # this scene becomes the current one.
    def initialize(&block)
      @register_block = block
    end

    def register_events
      @held_keys = []

      on :key_press do |key, mod|
        @held_keys << key
      end

      on :key_release do |key, mod|
        @held_keys.reject! { |i| i == key }
      end

      if @register_block
        instance_eval(&@register_block)
      else
        register
      end

      @exit = false
    end

    # Override this method in subclasses to register your own events
    def register
    end

    # @param [Symbol, Integer] val A symbol to find the key (its name)
    #                              or an integer (Ray::Event::KEY_*)
    #
    # @return [true, false] True if the user is holding key.
    def holding?(val)
      if val.is_a? Symbol
        val = key(val)
        @held_keys.any? { |o| val === o }
      elsif val.is_a? DSL::Matcher
        @held_keys.any? { |o| val === o }
      else
        @held_keys.include? val
      end
    end

    # Runs until you exit the scene.
    # This will also raise events if the mouse moves, ... allowing you
    # to directly listen to a such event.
    def run
      until @exit
        DSL::EventTranslator.translate_event(Ray::Event.new).each do |args|
          raise_event(*args)
        end

        @always_block.call if @always_block

        listener_runner.run

        if @need_render
          @need_render = false

          if @render_block
            @render_block.call(@window)
          else
            render(@window)
          end

          @window.flip
        end
      end
    end

    # Exits the scene, but does not pop the scene.
    #
    # You may want to call this if you pushed a new scene, to switch to
    # the new scene.
    def exit
      @exit = true
    end

    # Exits the scene and pops it (may not work as expected if the current
    # scene is not the last one)
    def exit!
      exit
      game.pop_scene
    end

    # Registers a block to be excuted as often as possible.
    def always(&block)
      @always_block = block
    end

    # Marks the scene should be redrawn.
    def need_render!
      @need_render = true
    end

    # Registers the block to draw the scene.
    #
    # Does nothing if no block is given, this method being called if you
    # didn't register any render block. You can thus override it in subclasses
    # instead of providing a block to it.
    #
    # @yield [window] Block to render this scene.
    # @yieldparam [Ray::Image] window The window you should draw on
    def render(win = nil, &block)
      if block_given?
        @render_block = block
      else
        # Do nothing
      end
    end

    # Pushes a scene in the stack, and exits that one
    def push_scene(scene)
      game.push_scene(scene)
      exit
    end

    def inspect
      "scene(:#{self.class.scene_name} => #{game.title.inspect})"
    end

    alias :pop_scene :exit!

    attr_accessor :game
    attr_accessor :window
  end
end
