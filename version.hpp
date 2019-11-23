#ifndef VERSION_HPP
#define VERSION_HPP
struct version{
    public:
        version();
        version(int, int);
        bool operator==(version &);
        int get_timestamp() const;
        void write_timestamp(int);
        int get_datacenterid() const;
        void write_datacenterid(int);
    private:
        int timestamp;
        int datacenter_id;
};
#endif