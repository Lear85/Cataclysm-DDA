#ifndef INVENTORY_UI_H
#define INVENTORY_UI_H

#include <limits>
#include <memory>

#include "color.h"
#include "cursesdef.h"
#include "enums.h"
#include "input.h"
#include "output.h"
#include "units.h"
#include "item_location.h"

class Character;

class item;
class item_category;
class item_location;

class player;

enum class navigation_mode : int {
    ITEM = 0,
    CATEGORY
};

enum class scroll_direction : int {
    FORWARD = 1,
    BACKWARD = -1
};

struct navigation_mode_data;
struct inventory_input;

class inventory_entry
{
    public:
        item_location location;

        size_t chosen_count = 0;
        long custom_invlet = LONG_MIN;

        inventory_entry( const item_location &location, size_t stack_size,
                         const item_category *custom_category = nullptr, bool enabled = true ) :
            location( location.clone() ),
            stack_size( stack_size ),
            custom_category( custom_category ),
            enabled( enabled ) {}

        inventory_entry( const inventory_entry &entry ) :
            location( entry.location.clone() ),
            chosen_count( entry.chosen_count ),
            custom_invlet( entry.custom_invlet ),
            stack_size( entry.stack_size ),
            custom_category( entry.custom_category ),
            enabled( entry.enabled ) {}

        inventory_entry operator=( const inventory_entry &rhs ) {
            location = rhs.location.clone();
            chosen_count = rhs.chosen_count;
            custom_invlet = rhs.custom_invlet;
            stack_size = rhs.stack_size;
            custom_category = rhs.custom_category;
            enabled = rhs.enabled;
            return *this;
        }

        inventory_entry( const item_location &location, const item_category *custom_category = nullptr,
                         bool enabled = true ) :
            inventory_entry( location, location ? 1 : 0, custom_category, enabled ) {}

        inventory_entry( const item_category *custom_category = nullptr ) :
            inventory_entry( item_location(), custom_category ) {}

        inventory_entry( const inventory_entry &entry, const item_category *custom_category ) :
            inventory_entry( entry ) {
            this->custom_category = custom_category;
        }

        bool operator==( const inventory_entry &other ) const;
        bool operator!=( const inventory_entry &other ) const {
            return !( *this == other );
        }

        operator bool() const {
            return !is_null();
        }
        /** Whether the entry is null (dummy) */
        bool is_null() const {
            return get_category_ptr() == nullptr;
        }
        /**
         * Whether the entry is an item.
         * item_location::valid() is way too expensive for mundane routines.
         */
        bool is_item() const {
            return location != item_location::nowhere;
        }
        /** Whether the entry is a category */
        bool is_category() const {
            return !is_null() && !is_item();
        }
        /** Whether the entry can be selected */
        bool is_selectable() const {
            return is_item() && enabled;
        }

        size_t get_stack_size() const {
            return stack_size;
        }

        size_t get_available_count() const;
        const item_category *get_category_ptr() const;
        long get_invlet() const;
        nc_color get_invlet_color() const;

    private:
        size_t stack_size;
        const item_category *custom_category;
        bool enabled = true;

};

class inventory_selector_preset
{
    public:
        inventory_selector_preset();

        /** Does this entry satisfy the basic preset conditions? */
        virtual bool is_shown( const item_location & ) const {
            return true;
        }

        /**
         * The reason why this entry cannot be selected.
         * @return Either the reason of denial or empty string if it's accepted.
         */
        virtual std::string get_denial( const item_location & ) const {
            return std::string();
        }
        /** Whether the first item is considered to go before the second. */
        virtual bool sort_compare( const item_location &lhs, const item_location &rhs ) const;
        /** Color that will be used to display the entry string. */
        virtual nc_color get_color( const inventory_entry &entry ) const;

        /** Text in the cell */
        std::string get_cell_text( const inventory_entry &entry, size_t cell_index ) const;
        /** Width of the cell */
        size_t get_cell_width( const inventory_entry &entry, size_t cell_index ) const;
        /** @return Whether the cell is a stub */
        bool is_stub_cell( const inventory_entry &entry, size_t cell_index ) const;
        /** Number of cells in the preset. */
        size_t get_cells_count() const {
            return cells.size();
        }

    protected:
        /** Text of the first column (default: item name) */
        virtual std::string get_caption( const inventory_entry &entry ) const;
        /**
         * Append a new cell to the preset.
         * @param func The function that returns text for the cell.
         * @param title Title of the cell.
         * @param stub The cell won't be "revealed" if it contains only this value
         */
        void append_cell( const std::function<std::string( const item_location & )> &func,
                          const std::string &title = std::string(),
                          const std::string &stub = std::string() );
        void append_cell( const std::function<std::string( const inventory_entry & )> &func,
                          const std::string &title = std::string(),
                          const std::string &stub = std::string() );

    private:
        struct cell_t {
            std::function<std::string( const inventory_entry & )> func;
            std::string title;
            std::string stub;

            cell_t( const std::function<std::string( const inventory_entry & )> &func,
                    const std::string &title, const std::string &stub ) :
                func( func ),
                title( title ),
                stub( stub ) {}
        };

        std::vector<cell_t> cells;
};

const inventory_selector_preset default_preset;

class inventory_column
{
    public:
        inventory_column( const inventory_selector_preset &preset = default_preset ) : preset( preset ) {
            cells.resize( preset.get_cells_count() );
        }

        virtual ~inventory_column() {}

        bool empty() const {
            return entries.empty();
        }
        /**
         * Can this column be activated?
         * @return Whether the column contains selectable entries.
         */
        virtual bool activatable() const;
        /** Is this column visible? */
        bool visible() const {
            return !empty() && visibility && preset.get_cells_count() > 0;
        }
        /**
         * Does this column allow selecting?
         * "Cosmetic" columns (list of selected items) can explicitly prohibit selecting.
         */
        virtual bool allows_selecting() const {
            return activatable();
        }

        size_t page_index() const {
            return page_of( page_offset );
        }

        size_t pages_count() const {
            return page_of( entries.size() + entries_per_page - 1 );
        }

        bool is_selected( const inventory_entry &entry ) const;

        /**
         * Does this entry belong to the selected category?
         * When @ref navigation_mode::ITEM is used it's equivalent to @ref is_selected().
         */
        bool is_selected_by_category( const inventory_entry &entry ) const;

        const inventory_entry &get_selected() const;
        std::vector<inventory_entry *> get_all_selected() const;

        inventory_entry *find_by_invlet( long invlet ) const;

        void draw( WINDOW *win, size_t x, size_t y ) const;

        void add_entry( const inventory_entry &entry );
        void remove_entry( const inventory_entry &entry );
        void move_entries_to( inventory_column &dest );
        void clear();

        void set_multiselect( bool multiselect ) {
            this->multiselect = multiselect;
        }

        void set_mode( navigation_mode mode ) {
            this->mode = mode;
        }

        void set_visibility( bool visibility ) {
            this->visibility = visibility;
        }

        void set_width( size_t width );
        void set_height( size_t height );
        size_t get_width() const;
        size_t get_height() const;
        /** Expands the column to fit the new entry. */
        void expand_to_fit( const inventory_entry &entry );
        /** Resets width to original (unchanged). */
        void reset_width();
        /** Returns next custom inventory letter. */
        long reassign_custom_invlets( const player &p, long min_invlet, long max_invlet );
        /** Reorder entries, repopulate titles, adjust to the new height. */
        virtual void prepare_paging();
        /**
         * Event handlers
         */
        virtual void on_input( const inventory_input &input );
        /** The entry has been changed. */
        virtual void on_change( const inventory_entry & ) {}
        /** The column has been activated. */
        virtual void on_activate() {
            active = true;
        }
        /** The column has been deactivated. */
        virtual void on_deactivate() {
            active = false;
        }

    protected:
        /**
         * Change the selection.
         * @param new_index Index of the entry to select.
         * @param dir If the entry is not selectable, move in the specified direction
         */
        void select( size_t new_index, scroll_direction dir );
        /**
         * Move the selection.
         * @param step Same as one in @ref select().
         */
        void move_selection( scroll_direction dir );
        void move_selection_page( scroll_direction dir );

        size_t next_selectable_index( size_t index, scroll_direction dir ) const;

        size_t page_of( size_t index ) const;
        size_t page_of( const inventory_entry &entry ) const;
        /**
         * Indentation of the entry.
         * @param cell_index Either left indent when it's zero, or a gap between cells.
         */
        size_t get_entry_indent( const inventory_entry &entry ) const;
        /** Overall cell width. If corresponding cell is not empty (its width is greater than zero),
         *  then a value returned by @ref get_entry_indent() is added to the result.
         */
        size_t get_entry_cell_width( const inventory_entry &entry, size_t cell_index ) const;
        /** Sum of the cell widths */
        size_t get_cells_width() const;

        std::string get_entry_denial( const inventory_entry &entry ) const;

        const inventory_selector_preset &preset;

        std::vector<inventory_entry> entries;
        navigation_mode mode = navigation_mode::ITEM;
        bool active = false;
        bool multiselect = false;
        bool paging_is_valid = false;
        bool visibility = true;

        size_t selected_index = 0;
        size_t page_offset = 0;
        size_t entries_per_page = std::numeric_limits<size_t>::max();
        size_t reserved_width = 0;

    private:
        struct cell_t {
            size_t current_width = 0;   /// Current cell widths (can be affected by set_width())
            size_t real_width = 0;      /// Minimal cell widths (to embrace all the entries nicely)

            bool visible() const {
                return current_width > 0;
            }
            /** @return Gap before the cell. Negative value means the cell is shrinked */
            int gap() const {
                return current_width - real_width;
            }
        };

        std::vector<cell_t> cells;

        /** @return Number of visible cells */
        size_t visible_cells() const;
};

class selection_column : public inventory_column
{
    public:
        selection_column( const std::string &id, const std::string &name );

        virtual bool activatable() const override {
            return inventory_column::activatable() && pages_count() > 1;
        }

        virtual bool allows_selecting() const override {
            return false;
        }

        virtual void prepare_paging() override;
        virtual void on_change( const inventory_entry &entry ) override;

    private:
        const std::unique_ptr<item_category> selected_cat;
};

class inventory_selector
{
    public:
        inventory_selector( const player &u, const inventory_selector_preset &preset = default_preset );
        ~inventory_selector() {}
        /** These functions add items from map / vehicles. */
        void add_character_items( Character &character );
        void add_map_items( const tripoint &target );
        void add_vehicle_items( const tripoint &target );
        void add_nearby_items( int radius = 1 );
        /** Assigns a title that will be shown on top of the menu. */
        void set_title( const std::string &title ) {
            this->title = title;
        }
        /** Assigns a hint. */
        void set_hint( const std::string &hint ) {
            this->hint = hint;
        }
        /** Specify whether the header should show stats (weight and volume). */
        void set_display_stats( bool display_stats ) {
            this->display_stats = display_stats;
        }
        /** @return true when the selector is empty. */
        bool empty() const;
        /** @return true when there are enabled entries to select. */
        bool has_available_choices() const;

    protected:
        const player &u;
        const inventory_selector_preset &preset;

        /**
         * The input context for navigation, already contains some actions for movement.
         * See @ref on_action.
         */
        input_context ctxt;

        const item_category *naturalize_category( const item_category &category,
                const tripoint &pos );

        void add_item( inventory_column &target_column,
                       const item_location &location,
                       size_t stack_size = 1,
                       const item_category *custom_category = nullptr );

        void add_items( inventory_column &target_column,
                        const std::function<item_location( item * )> &locator,
                        const std::vector<std::list<item *>> &stacks,
                        const item_category *custom_category = nullptr );

        inventory_input get_input();

        /** Given an action from the input_context, try to act according to it. */
        void on_input( const inventory_input &input );
        /** Entry has been changed */
        void on_change( const inventory_entry &entry );

        void prepare_layout( size_t client_width, size_t client_height );
        size_t get_layout_width() const;
        size_t get_layout_height() const;

        void resize_window( int width, int height );
        void refresh_window() const;
        void update();

        /** Tackles screen overflow */
        virtual void rearrange_columns( size_t client_width );
        /** Returns player for volume/weight numbers */
        virtual const player &get_player_for_stats() const {
            return u;
        }

        std::vector<std::string> get_stats() const;
        std::pair<std::string, nc_color> get_footer( navigation_mode m ) const;

        size_t get_header_height() const;
        size_t get_header_min_width() const;
        size_t get_footer_min_width() const;

        void draw_header( WINDOW *w ) const;
        void draw_footer( WINDOW *w ) const;
        void draw_columns( WINDOW *w ) const;
        void draw_frame( WINDOW *w ) const;

        /** @return an entry from @ref entries by its invlet */
        inventory_entry *find_entry_by_invlet( long invlet ) const;

        const std::vector<inventory_column *> &get_all_columns() const {
            return columns;
        }
        std::vector<inventory_column *> get_visible_columns() const;

        inventory_column &get_column( size_t index ) const;
        inventory_column &get_active_column() const {
            return get_column( active_column_index );
        }

        void set_active_column( size_t index );
        size_t get_columns_width( const std::vector<inventory_column *> &columns ) const;
        /** @return Percentage of the window occupied by columns */
        double get_columns_occupancy_ratio( size_t client_width ) const;
        /** @return Do the visible columns need to be center-aligned */
        bool are_columns_centered( size_t client_width ) const;
        /** @return Are visible columns wider than available width */
        bool is_overflown( size_t client_width ) const;

        bool is_active_column( const inventory_column &column ) const {
            return &column == &get_active_column();
        }

        void append_column( inventory_column &column );

        /**
         * Activates either previous or next column.
         * @param dir Forward - next column, backward - previous.
         */
        void toggle_active_column( scroll_direction dir );

        void refresh_active_column() {
            if( !get_active_column().activatable() ) {
                toggle_active_column( scroll_direction::FORWARD );
            }
        }
        void toggle_navigation_mode();
        void reassign_custom_invlets();

        /** Entry has been added */
        virtual void on_entry_add( const inventory_entry & ) {}

        const navigation_mode_data &get_navigation_data( navigation_mode m ) const;

    private:
        WINDOW_PTR w_inv;

        std::list<item_location> items;
        std::vector<inventory_column *> columns;

        std::string title;
        std::string hint;
        size_t active_column_index;
        std::list<item_category> categories;
        navigation_mode mode;

        inventory_column own_inv_column;     // Column for own inventory items
        inventory_column own_gear_column;    // Column for own gear (weapon, armor) items
        inventory_column map_column;         // Column for map and vehicle items

        int border = 0;                      // Width of the window border

        bool display_stats = true;
        bool layout_is_valid = false;
};

class inventory_pick_selector : public inventory_selector
{
    public:
        inventory_pick_selector( const player &p,
                                 const inventory_selector_preset &preset = default_preset ) :
            inventory_selector( p, preset ) {}

        item_location execute();
};

class inventory_multiselector : public inventory_selector
{
    public:
        inventory_multiselector( const player &p, const inventory_selector_preset &preset = default_preset,
                                 const std::string &selection_column_title = "" );
    protected:
        virtual void rearrange_columns( size_t client_width ) override;
        virtual void on_entry_add( const inventory_entry &entry ) override;

    private:
        std::unique_ptr<inventory_column> selection_col;
};

class inventory_compare_selector : public inventory_multiselector
{
    public:
        inventory_compare_selector( const player &p );
        std::pair<const item *, const item *> execute();

    protected:
        std::vector<inventory_entry *> compared;

        void toggle_entry( inventory_entry *entry );
};

class inventory_drop_selector : public inventory_multiselector
{
    public:
        inventory_drop_selector( const player &p,
                                 const inventory_selector_preset &preset = default_preset );
        std::list<std::pair<int, int>> execute();

    protected:
        std::map<const item *, int> dropping;
        mutable std::unique_ptr<player> dummy;

        const player &get_player_for_stats() const;
        /** Toggle item dropping */
        void set_drop_count( inventory_entry &entry, size_t count );
};

#endif
