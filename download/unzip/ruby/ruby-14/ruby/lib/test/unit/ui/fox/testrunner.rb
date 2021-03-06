# :nodoc:
#
# Author:: Nathaniel Talbott.
# Copyright:: Copyright (c) 2000-2002 Nathaniel Talbott. All rights reserved.
# License:: Ruby license.

require 'fox'
require 'test/unit/ui/testrunnermediator'
require 'test/unit/ui/testrunnerutilities'

include Fox

module Test
  module Unit
    module UI
      module Fox # :nodoc:

        # Runs a Test::Unit::TestSuite in a Fox UI. Obviously,
        # this one requires you to have Fox
        # (http://www.fox-toolkit.org/fox.html) and the Ruby
        # Fox extension (http://fxruby.sourceforge.net/)
        # installed.
        class TestRunner

          extend TestRunnerUtilities
          
          RED_STYLE = FXRGBA(0xFF,0,0,0xFF) #0xFF000000
          GREEN_STYLE = FXRGBA(0,0xFF,0,0xFF) #0x00FF0000
  
          # Creates a new TestRunner and runs the suite.
          def self.run(suite)
            new(suite).start
          end

          # Creates a new TestRunner for running the passed
          # suite.
          def initialize(suite)
            if (suite.respond_to?(:suite))
              @suite = suite.suite
            else
              @suite = suite
            end

            @red = false
          end
  
          # Begins the test run.
          def start
            setup_ui
            setup_mediator
            attach_to_mediator
            start_ui
          end

          def setup_mediator # :nodoc:
            @mediator = TestRunnerMediator.new(@suite)
            suite_name = @suite.to_s
            if ( @suite.kind_of?(Module) )
              suite_name = @suite.name
            end
            @suite_name_entry.text = suite_name
          end
          
          def attach_to_mediator # :nodoc:
            @mediator.add_listener(TestRunnerMediator::RESET, &method(:reset_ui))
            @mediator.add_listener(TestResult::FAULT, &method(:add_fault))
            @mediator.add_listener(TestResult::CHANGED, &method(:result_changed))
            @mediator.add_listener(TestRunnerMediator::STARTED, &method(:started))
            @mediator.add_listener(TestCase::STARTED, &method(:test_started))
            @mediator.add_listener(TestRunnerMediator::FINISHED, &method(:finished))
          end
  
          def start_ui # :nodoc:
            @application.create
            @window.show(PLACEMENT_SCREEN)
            @application.addTimeout(1) do
              @mediator.run_suite
            end
            @application.run
          end
          
          def stop # :nodoc:
            @application.exit(0)
          end
          
          def reset_ui(count) # :nodoc:
            @test_progress_bar.barColor = GREEN_STYLE
            @test_progress_bar.total = count
            @test_progress_bar.progress = 0
            @red = false
  
            @test_count_label.text = "0"
            @assertion_count_label.text = "0"
            @failure_count_label.text = "0"
            @error_count_label.text = "0"
  
            @fault_list.clearItems
          end
          
          def add_fault(fault) # :nodoc:
            if ( ! @red )
              @test_progress_bar.barColor = RED_STYLE
              @red = true
            end
            item = FaultListItem.new(fault)
            @fault_list.appendItem(item)
          end
          
          def show_fault(fault) # :nodoc:
            raw_show_fault(fault.long_display)
          end
          
          def raw_show_fault(string) # :nodoc:
            @detail_text.setText(string)
          end
          
          def clear_fault # :nodoc:
            raw_show_fault("")
          end
          
          def result_changed(result) # :nodoc:
            @test_progress_bar.progress = result.run_count
  
            @test_count_label.text = result.run_count.to_s
            @assertion_count_label.text = result.assertion_count.to_s
            @failure_count_label.text = result.failure_count.to_s
            @error_count_label.text = result.error_count.to_s

             # repaint now!
             @info_panel.repaint
             @application.flush
          end
          
          def started(result) # :nodoc:
            output_status("Started...")
          end
          
          def test_started(test_name)
            output_status("Running #{test_name}...")
          end
          
          def finished(elapsed_time)
            output_status("Finished in #{elapsed_time} seconds")
          end
          
          def output_status(string)
            @status_entry.text = string
            @status_entry.repaint
          end
  
          def setup_ui # :nodoc:
            @application = create_application
            create_tooltip(@application)

            @window = create_window(@application)
            
            @status_entry = create_entry(@window)
            
            main_panel = create_main_panel(@window)
            
            suite_panel = create_suite_panel(main_panel)
            create_label(suite_panel, "Suite:")
            @suite_name_entry = create_entry(suite_panel)
            create_button(suite_panel, "&Run\tRun the current suite", proc { @mediator.run_suite })
            
            @test_progress_bar = create_progress_bar(main_panel)
            
            @info_panel = create_info_panel(main_panel)
            create_label(@info_panel, "Tests:")
            @test_count_label = create_label(@info_panel, "0")
            create_label(@info_panel, "Assertions:")
            @assertion_count_label = create_label(@info_panel, "0")
            create_label(@info_panel, "Failures:")
            @failure_count_label = create_label(@info_panel, "0")
            create_label(@info_panel, "Errors:")
            @error_count_label = create_label(@info_panel, "0")
            
            list_panel = create_list_panel(main_panel)
            @fault_list = create_fault_list(list_panel)
            
            detail_panel = create_detail_panel(main_panel)
            @detail_text = create_text(detail_panel)
          end
          
          def create_application # :nodoc:
            app = FXApp.new("TestRunner", "Test::Unit")
            app.init([])
            app
          end
          
          def create_window(app)
            FXMainWindow.new(app, "Test::Unit TestRunner", nil, nil, DECOR_ALL, 0, 0, 450)
          end
          
          def create_tooltip(app)
            FXTooltip.new(app)
          end
  
          def create_main_panel(parent) # :nodoc:
            panel = FXVerticalFrame.new(parent, LAYOUT_FILL_X | LAYOUT_FILL_Y)
            panel.vSpacing = 10
            panel
          end
  
          def create_suite_panel(parent) # :nodoc:
            FXHorizontalFrame.new(parent, LAYOUT_SIDE_LEFT | LAYOUT_FILL_X)
          end
          
          def create_button(parent, text, action) # :nodoc:
            FXButton.new(parent, text).connect(SEL_COMMAND, &action)
          end
          
          def create_progress_bar(parent) # :nodoc:
            FXProgressBar.new(parent, nil, 0, PROGRESSBAR_NORMAL | LAYOUT_FILL_X)
          end
          
          def create_info_panel(parent) # :nodoc:
            FXMatrix.new(parent, 1, MATRIX_BY_ROWS | LAYOUT_FILL_X)
          end
          
          def create_label(parent, text)
            FXLabel.new(parent, text, nil, JUSTIFY_CENTER_X | LAYOUT_FILL_COLUMN)
          end
          
          def create_list_panel(parent) # :nodoc:
            FXHorizontalFrame.new(parent, LAYOUT_FILL_X | FRAME_SUNKEN | FRAME_THICK)
          end
          
          def create_fault_list(parent) # :nodoc:
            list = FXList.new(parent, 10, nil, 0, LIST_SINGLESELECT | LAYOUT_FILL_X) #, 0, 0, 0, 150)
            list.connect(SEL_COMMAND) do |sender, sel, ptr|
              if sender.retrieveItem(sender.currentItem).selected?
                show_fault(sender.retrieveItem(sender.currentItem).fault)
              else
                clear_fault
              end
            end
            list
          end
          
          def create_detail_panel(parent) # :nodoc:
            FXHorizontalFrame.new(parent, LAYOUT_FILL_X | LAYOUT_FILL_Y | FRAME_SUNKEN | FRAME_THICK)
          end
          
          def create_text(parent) # :nodoc:
            FXText.new(parent, nil, 0, TEXT_READONLY | LAYOUT_FILL_X | LAYOUT_FILL_Y)
          end
          
          def create_entry(parent) # :nodoc:
            entry = FXTextField.new(parent, 30, nil, 0, TEXTFIELD_NORMAL | LAYOUT_SIDE_BOTTOM | LAYOUT_FILL_X)
            entry.disable
            entry
          end
        end
  
        class FaultListItem < FXListItem # :nodoc: all
          attr_reader(:fault)
          def initialize(fault)
            super(fault.short_display)
            @fault = fault
          end
        end
      end
    end
  end
end

if __FILE__ == $0
  Test::Unit::UI::Fox::TestRunner.start_command_line_test
end
