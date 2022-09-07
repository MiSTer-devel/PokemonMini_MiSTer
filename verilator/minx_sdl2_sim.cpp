#include "Vminx.h"
#include "Vminx___024root.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "instruction_cycles.h"

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include "gl_utils.h"

#define VERBOSE 1

#if VERBOSE == 0
#define PRINTE(...) do{ } while ( false )
#define PRINTD(...) do{ } while ( false )
#elif VERBOSE == 1
#define PRINTE(...) do{ fprintf( stderr, __VA_ARGS__ ); } while( false )
#define PRINTD(...) do{ } while ( false )
#else
#define PRINTE(...) do{ fprintf( stderr, __VA_ARGS__ ); } while( false )
#define PRINTD(...) do{ fprintf( stdout, __VA_ARGS__ ); } while( false )
#endif


int min(int a, int b)
{
    return a < b? a: b;
}

enum
{
    BUS_IDLE      = 0x0,
    BUS_IRQ_READ  = 0x1,
    BUS_MEM_WRITE = 0x2,
    BUS_MEM_READ  = 0x3
};

bool data_sent = false;
bool irq_processing = false;
int irq_copy_complete_old = 0;
int num_cycles_since_sync = 0;
int reset_counter = 0;

bool gl_renderer_init(int buffer_width, int buffer_height)
{
    GLenum err = glewInit();
    if(err != GLEW_OK)
    {
        fprintf(stderr, "Error initializing GLEW.\n");
        return false;
    }

    int glVersion[2] = {-1, 1};
    glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
    glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

    gl_debug(__FILE__, __LINE__);

    printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);
    printf("Renderer used: %s\n", glGetString(GL_RENDERER));
    printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glClearColor(1.0, 0.0, 0.0, 1.0);

    // Create texture for presenting buffer to OpenGL
    GLuint buffer_texture;
    glGenTextures(1, &buffer_texture);
    glBindTexture(GL_TEXTURE_2D, buffer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, buffer_width, buffer_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    // Create vao for generating fullscreen triangle
    GLuint fullscreen_triangle_vao;
    glGenVertexArrays(1, &fullscreen_triangle_vao);


    // Create shader for displaying buffer
    static const char* fragment_shader =
        "\n"
        "#version 330\n"
        "\n"
        "uniform sampler2D buffer;\n"
        "noperspective in vec2 TexCoord;\n"
        "\n"
        "out vec3 outColor;\n"
        "\n"
        "void main(void){\n"
        "    float val = texture(buffer, TexCoord).r;\n"
        "    outColor = (1.0 - val) * vec3(183.0, 202.0, 183.0)/255.0 + val * vec3(4.0, 22.0, 4.0) / 255.0;\n"
        "}\n";

    static const char* vertex_shader =
        "\n"
        "#version 330\n"
        "\n"
        "noperspective out vec2 TexCoord;\n"
        "\n"
        "void main(void){\n"
        "\n"
        "    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
        "    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
        "    \n"
        "    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
        "}\n";

    GLuint shader_id = glCreateProgram();

    {
        //Create vertex shader
        GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(shader_vp, 1, &vertex_shader, 0);
        glCompileShader(shader_vp);
        validate_shader(shader_vp, vertex_shader);
        glAttachShader(shader_id, shader_vp);

        glDeleteShader(shader_vp);
    }

    {
        //Create fragment shader
        GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shader_fp, 1, &fragment_shader, 0);
        glCompileShader(shader_fp);
        validate_shader(shader_fp, fragment_shader);
        glAttachShader(shader_id, shader_fp);

        glDeleteShader(shader_fp);
    }

    glLinkProgram(shader_id);

    if(!validate_program(shader_id)){
        fprintf(stderr, "Error while validating shader.\n");
        glDeleteVertexArrays(1, &fullscreen_triangle_vao);
        return false;
    }

    glUseProgram(shader_id);

    GLint location = glGetUniformLocation(shader_id, "buffer");
    glUniform1i(location, 0);


    //OpenGL setup
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(fullscreen_triangle_vao);

    return true;
}

void gl_renderer_draw(int buffer_width, int buffer_height, void* buffer_data)
{
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0,
        buffer_width, buffer_height,
        GL_RED, GL_UNSIGNED_BYTE,
        buffer_data
    );
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

struct SimData
{
    Vminx* minx;
    VerilatedVcdC* tfp;

    uint64_t timestamp;
    uint64_t osc1_clocks;
    uint64_t osc1_next_clock;

    uint8_t* bios;
    uint8_t* memory;
    uint8_t* cartridge;

    size_t bios_file_size;
    size_t cartridge_file_size;

    uint8_t* bios_touched;
    uint8_t* cartridge_touched;
    uint8_t* instructions_executed;

    uint8_t fb_write_index;
    uint8_t framebuffers[768*8];
};

struct AudioBuffer
{
    uint8_t* data;
    size_t size;
    size_t read_position;
};

void sim_init(SimData* sim, const char* cartridge_path)
{
    FILE* fp = fopen("data/bios.min", "rb");
    fseek(fp, 0, SEEK_END);
    sim->bios_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);  /* same as rewind(f); */

    sim->bios = (uint8_t*) malloc(sim->bios_file_size);
    fread(sim->bios, 1, sim->bios_file_size, fp);
    fclose(fp);

    sim->bios_touched = (uint8_t*) calloc(sim->bios_file_size, 1);

    sim->memory = (uint8_t*) calloc(1, 4*1024);

    // Load a cartridge.
    sim->cartridge = (uint8_t*) calloc(1, 0x200000);

    fp = fopen(cartridge_path, "rb");
    fseek(fp, 0, SEEK_END);
    sim->cartridge_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);  /* same as rewind(f); */
    fread(sim->cartridge, 1, sim->cartridge_file_size, fp);
    fclose(fp);

    sim->cartridge_touched = (uint8_t*) calloc(1, sim->cartridge_file_size);
    sim->instructions_executed = (uint8_t*) calloc(1, 0x300);

    sim->fb_write_index = 0;
    memset(sim->framebuffers, 0x0, 8*768);

    sim->minx = new Vminx;
    sim->minx->clk = 0;
    sim->minx->reset = 1;
    sim->minx->clk_ce_4mhz = 1;
    sim->minx->eeprom_we = 0;

    sim->osc1_clocks = 4000000.0 / 32768.0 + 0.5;
    sim->osc1_next_clock = sim->osc1_clocks;

    sim->timestamp = 0;

    Verilated::traceEverOn(true);
    sim->tfp = nullptr;

    sim->minx->clk_rt_ce = 1;
}

void sim_dump_stop(SimData* sim)
{
    if(!sim->tfp) return;
    printf("Stopping dump.\n");

    sim->tfp->close();
    delete sim->tfp;
    sim->tfp = nullptr;
}

void sim_dump_eeprom(SimData* sim, const char* filepath)
{
    VlUnpacked<unsigned char, 8192> rom = sim->minx->rootp->minx__DOT__eeprom__DOT__rom;
    const uint8_t* data = rom.m_storage;
    FILE* fp = fopen(filepath, "wb");
    {
        fwrite(data, 1, 8192, fp);
    }
    fclose(fp);
}

void sim_dump_start(SimData* sim, const char* filepath)
{
    printf("Starting dump at timestamp: %llu.\n", sim->timestamp);
    if(sim->tfp)
        sim_dump_stop(sim);

    sim->tfp = new VerilatedVcdC;
    sim->minx->trace(sim->tfp, 99);  // Trace 99 levels of hierarchy
    //sim->tfp->rolloverMB(209715200);
    sim->tfp->open(filepath);
}

void eeprom_set_timestamp(uint8_t* eeprom, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    if (!eeprom) return;
    uint8_t checksum = year + month + day + hour + min + sec;
    eeprom[0x1FF6] = 0x00;
    eeprom[0x1FF7] = 0x00;
    eeprom[0x1FF8] = 0x00;
    eeprom[0x1FF9] = year;
    eeprom[0x1FFA] = month;
    eeprom[0x1FFB] = day;
    eeprom[0x1FFC] = hour;
    eeprom[0x1FFD] = min;
    eeprom[0x1FFE] = sec;
    eeprom[0x1FFF] = checksum;
}

void sim_load_eeprom(SimData* sim, const char* filepath)
{
    uint8_t* eeprom = sim->minx->rootp->minx__DOT__eeprom__DOT__rom.m_storage;
    {
        strncpy((char*)eeprom, "GBMN", 4);
        eeprom[0x1FF2] = 0x01;
        eeprom[0x1FF3] = 0x03;
        eeprom[0x1FF4] = 0x01;
        eeprom[0x1FF5] = 0x1F;
        //FILE* fp = fopen(filepath, "rb");
        //fread(eeprom, 1, 8192, fp);
        //fclose(fp);
    }
    // @todo: Try initializing just a few required fields, like the GBMN and
    // see if that's sufficient for accepting the set datetime.

    time_t tim = time(NULL);
    struct tm* now = localtime(&tim);
    eeprom_set_timestamp(eeprom, now->tm_year % 100, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);

    // @note: The commented out part is not required; these already have these values.
    //sim->minx->rootp->minx__DOT__rtc__DOT__timer = 0;
    //sim->minx->rootp->minx__DOT__rtc__DOT__reg_enabled = 1;
    sim->minx->rootp->minx__DOT__system_control__DOT__reg_system_control[2] |= 2;
}

void simulate_steps(SimData* sim, int n_steps, AudioBuffer* audio_buffer = nullptr)
{
    uint8_t frame_complete_latch = sim->minx->frame_complete;
    for(int i = 0; i < n_steps && !Verilated::gotFinish(); ++i)
    {
        sim->minx->clk = 1;
        sim->minx->eval();
        if(sim->timestamp == sim->osc1_next_clock)
        {
            sim->minx->clk_rt = !sim->minx->clk_rt;
            sim->minx->eval();
            if(sim->tfp) sim->tfp->dump(sim->timestamp);
            sim->osc1_next_clock += sim->osc1_clocks;
        }
        else if(sim->tfp) sim->tfp->dump(sim->timestamp);
        sim->timestamp++;

        sim->minx->clk = 0;
        sim->minx->eval();
        if(sim->timestamp == sim->osc1_next_clock)
        {
            sim->minx->clk_rt = !sim->minx->clk_rt;
            sim->minx->eval();
            if(sim->tfp) sim->tfp->dump(sim->timestamp);
            sim->osc1_next_clock += sim->osc1_clocks;
        }
        else if(sim->tfp) sim->tfp->dump(sim->timestamp);
        sim->timestamp++;

        if(sim->minx->address_out == 0xAB)
            sim_load_eeprom(sim, "eeprom000.bin");


        if(audio_buffer)
        {
            uint8_t volume = sim->minx->sound_volume;
            uint8_t sound_pulse = sim->minx->sound_pulse;
            uint8_t multiplier = (volume == 0)? 0: ((volume == 3)? 255: 127);
            //audio_buffer->data[i] = (2 * sound_pulse - 1) * multiplier;
            audio_buffer->data[i] = sound_pulse * multiplier;
            //if(audio_buffer->data[i] < 0) --audio_buffer->data[i];
        }

        if(sim->minx->frame_complete && !frame_complete_latch)
        {
            if(sim->minx->rootp->minx__DOT__lcd__DOT__display_enabled)
            {
                for (int yC=0; yC<8; yC++)
                {
                    for (int xC=0; xC<96; xC++)
                    {
                        uint8_t data = sim->minx->rootp->minx__DOT__lcd__DOT__all_pixels_on_enabled ?
                            0xFF:
                            sim->minx->rootp->minx__DOT__lcd__DOT__invert_pixels_enabled?
                                sim->minx->rootp->minx__DOT__lcd__DOT__lcd_data[yC * 132 + xC] ^ 0xFF:
                                sim->minx->rootp->minx__DOT__lcd__DOT__lcd_data[yC * 132 + xC];
                        sim->framebuffers[768 * sim->fb_write_index + yC * 96 + xC] = data;
                    }
                }
            }
            else memset(sim->framebuffers + 768 * sim->fb_write_index, 0, 96*8);
            sim->fb_write_index = (sim->fb_write_index + 1) % 8;
        }
        frame_complete_latch = sim->minx->frame_complete;

        if(sim->minx->rootp->minx__DOT__irq_copy_complete && irq_copy_complete_old == 0)
        {
            irq_copy_complete_old = 1;
            PRINTD("Copy complete %d.\n", sim->timestamp / 2);
        }
        else if(!sim->minx->rootp->minx__DOT__irq_copy_complete) irq_copy_complete_old = 0;

        // At rising edge of clock
        data_sent = false;


        // Check for errors
        {
            if(sim->minx->rootp->minx__DOT__cpu__DOT__state == 2 && sim->minx->pl == 0 && !sim->minx->bus_ack)
            {
                if(sim->minx->rootp->minx__DOT__cpu__DOT__microaddress == 0 &&
                   sim->minx->rootp->minx__DOT__cpu__DOT__extended_opcode != 0x1AE
                ){
                    PRINTE("** Instruction 0x%x not implemented at 0x%x, timestamp: %llu**\n", sim->minx->rootp->minx__DOT__cpu__DOT__extended_opcode, sim->minx->rootp->minx__DOT__cpu__DOT__top_address, sim->timestamp);
                }
            }

            //if(
            //    (sim->minx->sync == 1) &&
            //    (sim->minx->pk == 0) &&
            //    sim->minx->iack == 0 &&
            //    sim->minx->rootp->minx__DOT__clk_ce &&
            //    !sim->minx->bus_ack)
            //{
            //    printf("^ 0x%x\n", sim->minx->address_out);
            //}

            if(
                (sim->minx->sync == 1) &&
                (sim->minx->pl == 0) &&
                (sim->minx->rootp->minx__DOT__cpu__DOT__micro_op & 0x1000) &&
                sim->minx->iack == 0 &&
                sim->minx->rootp->minx__DOT__clk_ce &&
                !sim->minx->bus_ack)
            {
                if(irq_processing)
                    irq_processing = false;
                else
                {
                    uint8_t num_cycles        = num_cycles_since_sync;
                    uint16_t extended_opcode  = sim->minx->rootp->minx__DOT__cpu__DOT__extended_opcode;
                    uint8_t num_cycles_actual = instruction_cycles[2*extended_opcode];
                    uint8_t num_cycles_actual_branch = instruction_cycles[2*extended_opcode+1];


                    if(num_cycles != num_cycles_actual)
                        if(num_cycles != num_cycles_actual_branch || num_cycles_actual_branch == 0)
                            PRINTE(" ** Discrepancy found in number of cycles of instruction 0x%x: %d, %d, timestamp: %llu** \n", extended_opcode, num_cycles, num_cycles_actual, sim->timestamp);

                    //if(sim->minx->address_out == 0x4C5C)
                    //    printf("^ address: 0x%x, A: 0x%x\n", 0x4C5C, sim->minx->rootp->minx__DOT__cpu__DOT__BA & 0xFF);

                    //if(!sim->instructions_executed[extended_opcode])
                    //    printf("Instruction 0x%x executed for the first time, at 0x%x, timestamp: %llu.\n", extended_opcode, sim->minx->rootp->minx__DOT__cpu__DOT__top_address, sim->timestamp);
                    sim->instructions_executed[extended_opcode] = 1;
                }
            }

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_addressing_error == 1)
                PRINTE(" ** Addressing not implemented error: 0x%llx, timestamp: %llu** \n", (sim->minx->rootp->minx__DOT__cpu__DOT__micro_op & 0x3F00000) >> 20, sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_jump_error == 1)
                PRINTE(" ** Jump not implemented error, 0x%llx, timestamp: %llu** \n", (sim->minx->rootp->minx__DOT__cpu__DOT__micro_op & 0x7C000) >> 14, sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_data_out_error == 1)
                PRINTE(" ** Data-out not implemented error, timestamp: %llu** \n", sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_mov_src_error == 1)
                PRINTE(" ** Mov src not implemented error, timestamp: %llu** \n", sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_write_error == 1)
                PRINTE(" ** Write not implemented error, timestamp: %llu** \n", sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__alu_op_error == 1)
                PRINTE(" ** Alu not implemented error, timestamp: %llu** \n", sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_alu_pack_ops_error == 1)
                PRINTE(" ** Alu packed operations not implemented error, sim->timestamp: %llu, 0x%x** \n", sim->timestamp, sim->minx->rootp->minx__DOT__cpu__DOT__top_address);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__not_implemented_divzero_error == 1)
                PRINTE(" ** Division by zero exception not implemented error, sim->timestamp: %llu**\n", sim->timestamp);

            if(sim->minx->rootp->minx__DOT__cpu__DOT__SP > 0x2000 && sim->minx->pl == 0)
            {
                PRINTE(" ** Stack overflow, timestamp: %llu**\n", sim->timestamp);
                break;
            }
        }

        //static bool once = false;
        //if(sim->minx->rootp->minx__DOT__cpu__DOT__extended_opcode == 0x1AE)
        //{
        //    if(!once) printf("timestamp: %llu\n", sim->timestamp);
        //    once = true;
        //}

        //if(sim->minx->rootp->minx__DOT__sound__DOT__reg_sound_volume == 3)
        //    printf("%llu\n", sim->timestamp);

        //if(
        //    sim->minx->rootp->minx__DOT__cpu__DOT__postpone_exception == 1 &&
        //    sim->minx->rootp->iack == 1 &&
        //    sim->minx->rootp->minx__DOT__cpu__DOT__NB > 0
        //){
        //    if(!sim->tfp)
        //        sim_dump_start(sim, "temp.vcd");
        //}
        //if(sim->timestamp == 82824492 - 10000000)
        //    sim_dump_start(sim, "sim.vcd");

        //if(sim->timestamp == 82824492 + 1000000)
        //    sim_dump_stop(sim);

        if(sim->minx->reset == 1 && reset_counter < 8)
            ++reset_counter;
        else if(reset_counter >= 8)
        {
            sim->minx->reset = 0;
            reset_counter = 0;
        }

        //if(sim->minx->address_out == 0x1479 && sim->minx->bus_status == BUS_MEM_WRITE && sim->minx->write)
        //{
        //    printf("%llu, 0x%x, 0x%x\n", sim->timestamp, sim->minx->rootp->minx__DOT__cpu__DOT__top_address, sim->minx->data_out);
        //}

        if(sim->timestamp > 258 && sim->minx->iack == 1 && sim->minx->pl == 0)// && sim->minx->sync)
        {
            irq_processing = true;
        }

        if(sim->minx->bus_status == BUS_MEM_READ && sim->minx->pl == 0) // Check if PL=0 just to reduce spam.
        {
            // memory read
            if(sim->minx->address_out < 0x1000)
            {
                // read from bios
                sim->bios_touched[sim->minx->address_out & (sim->bios_file_size - 1)] = 1;
                sim->minx->data_in = *(sim->bios + (sim->minx->address_out & (sim->bios_file_size - 1)));
            }
            else if(sim->minx->address_out < 0x2000)
            {
                // read from ram
                uint32_t address = sim->minx->address_out & 0xFFF;
                sim->minx->data_in = *(uint8_t*)(sim->memory + address);
            }
            else
            {
                // read from cartridge
                sim->cartridge_touched[(sim->minx->address_out & 0x1FFFFF) & (sim->cartridge_file_size - 1)] = 1;
                sim->minx->data_in = *(uint8_t*)(sim->cartridge + (sim->minx->address_out & 0x1FFFFF));
            }

            data_sent = true;
        }
        else if(sim->minx->bus_status == BUS_MEM_WRITE && sim->minx->write)
        {
            //if(sim->minx->address_out == 0x2085 && sim->minx->data_out > 0)
            //    printf("0x%x: 0x%x, timestamp: %d\n", sim->minx->rootp->minx__DOT__cpu__DOT__top_address, sim->minx->data_out, sim->timestamp);

            // memory write
            if(sim->minx->address_out < 0x1000)
            {
                PRINTD("Program trying to write to bios at 0x%x, timestamp: %llu\n", sim->minx->address_out, sim->timestamp);
            }
            else if(sim->minx->address_out < 0x2000)
            {
                // write to ram
                uint32_t address = sim->minx->address_out & 0xFFF;
                *(uint8_t*)(sim->memory + address) = sim->minx->data_out;
            }
            else
            {
                PRINTD("Program trying to write to cartridge at 0x%x, timestamp: %llu\n", sim->minx->address_out, sim->timestamp);
            }

            data_sent = true;
        }

        if(sim->minx->rootp->minx__DOT__clk_ce)
        {
            if(sim->minx->sync && sim->minx->pl == 1)
                num_cycles_since_sync = 0;

            if(sim->minx->pl == 1 && !sim->minx->bus_ack)
                ++num_cycles_since_sync;
        }
    }
}
// Contrast level on light and dark pixel
const uint8_t contrast_level_map[64*2] = {
      0,   4,   //  0 (0x00)
      0,   4,   //  1 (0x01)
      0,   4,   //  2 (0x02)
      0,   4,   //  3 (0x03)
      0,   6,   //  4 (0x04)
      0,  11,   //  5 (0x05)
      0,  17,   //  6 (0x06)
      0,  24,   //  7 (0x07)
      0,  31,   //  8 (0x08)
      0,  40,   //  9 (0x09)
      0,  48,   // 10 (0x0A)
      0,  57,   // 11 (0x0B)
      0,  67,   // 12 (0x0C)
      0,  77,   // 13 (0x0D)
      0,  88,   // 14 (0x0E)
      0,  99,   // 15 (0x0F)
      0, 110,   // 16 (0x10)
      0, 122,   // 17 (0x11)
      0, 133,   // 18 (0x12)
      0, 146,   // 19 (0x13)
      0, 158,   // 20 (0x14)
      0, 171,   // 21 (0x15)
      0, 184,   // 22 (0x16)
      0, 198,   // 23 (0x17)
      0, 212,   // 24 (0x18)
      0, 226,   // 25 (0x19)
      0, 240,   // 26 (0x1A)
      0, 255,   // 27 (0x1B)
      2, 255,   // 28 (0x1C)
      5, 255,   // 29 (0x1D)
     10, 255,   // 30 (0x1E)
     15, 255,   // 31 (0x1F)
     21, 255,   // 32 (0x20)
     27, 255,   // 33 (0x21)
     34, 255,   // 34 (0x22)
     41, 255,   // 35 (0x23)
     48, 255,   // 36 (0x24)
     56, 255,   // 37 (0x25)
     64, 255,   // 38 (0x26)
     73, 255,   // 39 (0x27)
     81, 255,   // 40 (0x28)
     90, 255,   // 41 (0x29)
    100, 255,   // 42 (0x2A)
    109, 255,   // 43 (0x2B)
    119, 255,   // 44 (0x2C)
    129, 255,   // 45 (0x2D)
    139, 255,   // 46 (0x2E)
    149, 255,   // 47 (0x2F)
    160, 255,   // 48 (0x30)
    171, 255,   // 49 (0x31)
    182, 255,   // 50 (0x32)
    193, 255,   // 51 (0x33)
    204, 255,   // 52 (0x34)
    216, 255,   // 53 (0x35)
    228, 255,   // 54 (0x36)
    240, 255,   // 55 (0x37)
    240, 255,   // 56 (0x38)
    240, 255,   // 57 (0x39)
    240, 255,   // 58 (0x3A)
    240, 255,   // 59 (0x3B)
    240, 255,   // 60 (0x3C)
    240, 255,   // 61 (0x3D)
    240, 255,   // 62 (0x3E)
    240, 255,   // 63 (0x3F)
};

uint8_t* get_lcd_image(const SimData* sim)
{
    uint8_t contrast = sim->minx->rootp->minx__DOT__lcd__DOT__contrast;
    uint8_t* image_data = new uint8_t[96*64];

    for (int yC=0; yC<8; yC++)
    {
        for (int xC=0; xC<96; xC++)
        {
            uint8_t data = sim->minx->rootp->minx__DOT__lcd__DOT__lcd_data[yC * 132 + xC];
            //uint8_t data = sim->memory[yC * 96 + xC];
            for(int i = 0; i < 8; ++i)
            {
                int idx = 96 * (63 - 8 * yC - i) + xC;
                image_data[idx] = ((~data >> i) & 1)? contrast_level_map[2*contrast]: contrast_level_map[2*contrast + 1];
            }
        }
    }

    return image_data;
}

uint8_t* render_framebuffers(const SimData* sim)
{
    uint8_t contrast = sim->minx->rootp->minx__DOT__lcd__DOT__contrast;

    uint8_t* image_data = new uint8_t[96*64];

    //static uint8_t levels_covered[16] = {};

    for (int yC=0; yC<8; yC++)
    {
        for (int xC=0; xC<96; xC++)
        {
            uint8_t fb_idx = (sim->fb_write_index + 7) % 8;
            uint8_t d0 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 6) % 8;
            uint8_t d1 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 5) % 8;
            uint8_t d2 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 4) % 8;
            uint8_t d3 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 3) % 8;
            uint8_t d4 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 2) % 8;
            uint8_t d5 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            fb_idx = (sim->fb_write_index + 1) % 8;
            uint8_t d6 = sim->framebuffers[768 * fb_idx + yC * 96 + xC];
            for(int i = 0; i < 8; ++i)
            {
                int idx = 96 * (63 - 8 * yC - i) + xC;
                float output = 0.0;
                output += ((d0 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                output += ((d1 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                output += ((d2 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                output += ((d3 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                //output += ((d4 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                //output += ((d5 >> i) & 1)? contrast_level_map[2*contrast+1]: contrast_level_map[2*contrast];
                //output += 1.0 * (((~d6 >> i) & 1)? 255.0: 255.0 * (1.0 - (float)contrast / 0x20));
                image_data[idx] = output / 4.0;
            }
        }
    }

    //printf("%d%d%d%d\n", frames01_same, frames12_same, frames23_same, frames34_same);

    //for(int i = 0; i < 16; ++i)
    //    printf("%d", levels_covered[i]);
    //printf("\n");

    return image_data;
}

void audio_callback(void* userdata, uint8_t* stream, int len)
{
    memset(stream, 0, len);
    AudioBuffer* audio_buffer = (AudioBuffer*) userdata;
    for(int i = 0; i < len; ++i)
        ((uint8_t*)stream)[i] = audio_buffer->data[(91 * i + audio_buffer->read_position) % audio_buffer->size];
    audio_buffer->read_position += 91 * len;
    audio_buffer->read_position = audio_buffer->read_position % audio_buffer->size;
}

// @todo: Create a call stack for keeping track call/return problems.
// @todo: Interpolate frames like in MiSTer module and experiment.
int main(int argc, char** argv)
{
    size_t num_sim_steps = 150000;

    SimData sim;
    // Problem with display starting 2 pixels from the left. This is due to a
    // difference in how the LCD controller is implemented. In e.g. PokeMini it
    // is implemented in such a way that if End RWM mode is issued, it always
    // sets the column to the cached value when Start RWM mode was issued,
    // which is initialized to 0 if Start was not issued. 6shades calls End
    // without Start.
    //const char* rom_filepath = "data/badapple_2mb.min";
    //const char* rom_filepath = "data/6shades.min";
    const char* rom_filepath = "data/party_j.min";
    //const char* rom_filepath = "data/pokemon_party_mini_u.min";
    //const char* rom_filepath = "data/pokemon_pinball_mini_u.min";
    //const char* rom_filepath = "data/pokemon_puzzle_collection_u.min";
    //const char* rom_filepath = "data/pokemon_zany_cards_u.min";
    //const char* rom_filepath = "data/pichu_bros_mini_j.min";
    //const char* rom_filepath = "data/pokemon_anime_card_daisakusen_j.min";
    //const char* rom_filepath = "data/snorlaxs_lunch_time_j.min";
    //const char* rom_filepath = "data/pokemon_shock_tetris_j.min";
    //const char* rom_filepath = "data/togepi_no_daibouken_j.min";
    //const char* rom_filepath = "data/pokemon_race_mini_j.min";
    //const char* rom_filepath = "data/pokemon_sodateyasan_mini_j.min";
    //const char* rom_filepath = "data/pokemon_puzzle_collection_j.min";
    //const char* rom_filepath = "data/pokemon_puzzle_collection_vol2_j.min";
    //const char* rom_filepath = "data/pokemon_pinball_mini_j.min";
    sim_init(&sim, rom_filepath);

    // Create window and gl context, and game controller
    int window_width = 960/2;
    int window_height = 640/2;

    SDL_Window* window;
    SDL_GLContext gl_context;
    {
        if(SDL_Init(SDL_INIT_VIDEO| SDL_INIT_AUDIO) < 0)
        {
            fprintf(stderr, "Error initializing SDL. SDL_Error: %s\n", SDL_GetError());
            return -1;
        }


        // Use OpenGL 3.1 core
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        //SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

        window = SDL_CreateWindow(
            "Vectron",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            window_width, window_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
        );

        if(!window)
        {
            fprintf(stderr, "Error creating SDL window. SDL_Error: %s\n", SDL_GetError());
            SDL_Quit();
            return -1;
        }

        gl_context = SDL_GL_CreateContext(window);
        if(!gl_context)
        {
            fprintf(stderr, "Error creating SDL GL context. SDL_Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        int r, g, b, a, m, s;
        SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
        SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
        SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
        SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
        SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &m);
        //SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &s);

        SDL_GL_SetSwapInterval(1);
    }

    int drawable_width, drawable_height;
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
    gl_renderer_init(96, 64);

    AudioBuffer sim_audio_buffer;
    sim_audio_buffer.size          = num_sim_steps;
    sim_audio_buffer.data          = (uint8_t*)malloc(num_sim_steps);
    sim_audio_buffer.read_position = 0;

    SDL_AudioSpec audio_spec_want;
    SDL_memset(&audio_spec_want, 0, sizeof(audio_spec_want));

    audio_spec_want.freq     = 44100;
    audio_spec_want.format   = AUDIO_U8;
    audio_spec_want.channels = 1;
    audio_spec_want.samples  = 512;
    audio_spec_want.callback = audio_callback;
    audio_spec_want.userdata = (void*) &sim_audio_buffer;

    SDL_AudioSpec audio_spec;
    SDL_AudioDeviceID audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec_want, &audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if(!audio_device_id)
    {
        fprintf(stderr, "Error creating SDL audio device. SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudioDevice(audio_device_id, 0);

    uint64_t cpu_frequency = SDL_GetPerformanceFrequency();
    uint64_t current_clock = SDL_GetPerformanceCounter();

    bool sim_is_running = true;
    bool program_is_running = true;
    bool dump_sim = false;
    int eeprom_dump_id = 0;
    while(program_is_running)
    {
        //printf("%d, %d\n", sim.minx->rootp->minx__DOT__rtc__DOT__timer, sim.minx->rootp->minx__DOT__eeprom__DOT__rom.m_storage[0x1FF6]);
        // Process input
        SDL_Event sdl_event;
        while(SDL_PollEvent(&sdl_event) != 0)
        {
            // Keyboard input
            if(sdl_event.type == SDL_KEYDOWN)
            {
                if(sdl_event.key.keysym.sym == SDLK_p)
                {
                    sim_is_running = !sim_is_running;
                    continue;
                }
                else if(sdl_event.key.keysym.sym == SDLK_d)
                {
                    if(!dump_sim)
                    {
                        dump_sim = true;
                        sim_dump_start(&sim, "sim.vcd");
                    }
                    else
                    {
                        dump_sim = false;
                        sim_dump_stop(&sim);
                    }
                }
                else if(sdl_event.key.keysym.sym == SDLK_e)
                {
                    char filename[256];
                    snprintf(filename, 256, "eeprom%03d.bin", eeprom_dump_id++);
                    sim_dump_eeprom(&sim, filename);
                }
                else
                {
                    switch(sdl_event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        program_is_running = false;
                        break;
                    case SDLK_UP:
                        sim.minx->keys_active |= 0x08;
                        break;
                    case SDLK_DOWN:
                        sim.minx->keys_active |= 0x10;
                        break;
                    case SDLK_RIGHT:
                        sim.minx->keys_active |= 0x40;
                        break;
                    case SDLK_LEFT:
                        sim.minx->keys_active |= 0x20;
                        break;
                    case SDLK_x: // A
                        sim.minx->keys_active |= 0x01;
                        break;
                    case SDLK_z: // B
                        sim.minx->keys_active |= 0x02;
                        break;
                    case SDLK_r: // reset
                        sim.minx->reset = 1;
                        break;
                    case SDLK_s: // C
                    case SDLK_c:
                        sim.minx->keys_active |= 0x04;
                        break;
                    case SDLK_t: // Shock
                    case SDLK_j:
                        sim.minx->keys_active |= 0x100;
                        break;
                    case SDLK_b: // Power
                        sim.minx->keys_active |= 0x80;
                        break;
                    default:
                        break;
                    }
                }
            }
            else if(sdl_event.type == SDL_KEYUP)
            {
                switch(sdl_event.key.keysym.sym){
                case SDLK_UP:
                    sim.minx->keys_active &= ~0x08;
                    break;
                case SDLK_DOWN:
                    sim.minx->keys_active &= ~0x10;
                    break;
                case SDLK_RIGHT:
                    sim.minx->keys_active &= ~0x40;
                    break;
                case SDLK_LEFT:
                    sim.minx->keys_active &= ~0x20;
                    break;
                case SDLK_x: // A
                    sim.minx->keys_active &= ~0x01;
                    break;
                case SDLK_z: // B
                    sim.minx->keys_active &= ~0x02;
                    break;
                case SDLK_s: // C
                case SDLK_c:
                    sim.minx->keys_active &= ~0x04;
                    break;
                case SDLK_t: // Shock
                case SDLK_j:
                    sim.minx->keys_active &= ~0x100;
                    break;
                case SDLK_b: // Power
                    sim.minx->keys_active &= ~0x80;
                    break;
                default:
                    break;
                }
            }
            else if(sdl_event.type == SDL_QUIT)
            {
                program_is_running = false;
            }
            else if(sdl_event.type == SDL_WINDOWEVENT)
            {
                if(sdl_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
                }
                else if(sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    sim_is_running = false;
                else if(sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                    sim_is_running = true;
            }
        }


        uint64_t new_clock = SDL_GetPerformanceCounter();
        double frame_sec = double(new_clock - current_clock) / cpu_frequency;
        current_clock = new_clock;
        //printf("%f\n", 4000000 * frame_sec);

        if(sim_is_running)
            simulate_steps(&sim, min(num_sim_steps, (int)4000000 * frame_sec), &sim_audio_buffer);
        uint8_t* lcd_image = render_framebuffers(&sim);
        //uint8_t* lcd_image = get_lcd_image(&sim);
        gl_renderer_draw(96, 64, lcd_image);
        delete[] lcd_image;

        SDL_GL_SwapWindow(window);
    }

    sim_dump_stop(&sim);

    SDL_CloseAudioDevice(audio_device_id);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    size_t total_touched = 0;
    for(size_t i = 0; i < sim.bios_file_size; ++i)
        total_touched += sim.bios_touched[i];
    printf("%zu bytes out of total %zu read from bios.\n", total_touched, sim.bios_file_size);

    total_touched = 0;
    for(size_t i = 0; i < sim.cartridge_file_size; ++i)
        total_touched += sim.cartridge_touched[i];
    printf("%zu bytes out of total %zu read from cartridge.\n", total_touched, sim.cartridge_file_size);

    total_touched = 0;
    for(size_t i = 0; i < 0x300; ++i)
        total_touched += sim.instructions_executed[i];
    printf("%zu instructions out of total 608 executed.\n", total_touched);

    return 0;
}
