#include <stdio.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>
#include <Magick++.h>
#include <zbar.h>
#include <dirent.h>
#define STR(s) #s

using namespace std;

zbar::ImageScanner scanner;
bool ready = false;

list<string> split(string str, char del) {
  // declaring temp string to store the curr "word" upto del
  string temp = "";
  list<string> list;
  
  for (int i = 0; i < (int) str.size(); i++) {
    // If cur char is not del, then append it to the cur "word", otherwise
    // you have completed the word, print it, and start a new word.
    if (str[i] != del) {
      temp += str[i];
    } else {
      list.insert(list.begin(), temp);
      temp = "";
    }
  }
  
  return list;
}

string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

    if (!pipe) {
      throw runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }

    return result;
}

void decode(string filename) {
  Magick::Image src(filename); 

  int width = src.columns();   // extract dimensions
  int height = src.rows();

  Magick::Blob blob;  

  src.write(&blob, "GRAY", 8);

  const void *raw = blob.data();

  zbar::Image image(width, height, "Y800", raw, width * height);

  int n = scanner.scan(image);

  for(
    zbar::Image::SymbolIterator symbol = image.symbol_begin();
    symbol != image.symbol_end();
    ++symbol
  ) {
    cout << "decoded " << symbol->get_type_name() << " symbol \"" << symbol->get_data() << '"' << endl;
  }

  image.set_data(NULL, 0);
}

void write_frames() {
  exec("ffmpeg -loglevel quiet -i http://192.168.100.2:8080/video -r 5 -t 7 -f image2 -vcodec mjpeg frames/%d.jpg");
}

void decode_frames() {
  const char * path = "frames";
  DIR *dir;
  struct dirent *ent;

  while (true) {
    string result = exec("ls frames -al -f --ignore='*read*' | sort -k5 --numeric-sort | grep .jpg");

    list<string> list = split(result, '\n');

    if (list.size() == 0 && ready) break;

    for (string item : list) {
      if (item.find("read") == string::npos) {
        rename(string("frames/" + item).c_str(), string("frames/read-" + item).c_str());
      }

      cout << item << endl;
    }

    this_thread::sleep_for(chrono::milliseconds(500));
  }
}

int main() {
  #ifdef MAGICK_HOME
    Magick::InitializeMagick(MAGICK_HOME);
  #endif

  scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
 
  chrono::system_clock::time_point start = chrono::system_clock::now();

  future<void> write_frames_promise(async(write_frames));
  future<void> decode_frames_promise(async(decode_frames));

  write_frames_promise.get();

  ready = true;

  cout << "write_frames_promise finished" << endl;

  decode_frames_promise.get();

  cout << "decode_frames_promise finished" << endl;

  auto end = chrono::system_clock::now();

  auto diff = chrono::duration_cast<chrono::seconds>(end - start).count();

  cout << "Time: " << diff << endl;

  exec("rm -rf frames/*");

  return 0;
}