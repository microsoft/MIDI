using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NetworkMidi2ServerProto.SdkProto
{
    // this is a primative form of a utility I want to have in the client SDK

    public class UmpFactory
    {
        private UInt32[] _words = new UInt32[4];
        private int _currentWordIndex = 0;

        public Queue<Ump> Umps { get; private set; }

        public UmpFactory()
        {
            Umps = new Queue<Ump>();
        }

        public void AddMidiWord(UInt32 word)
        {
            _words[_currentWordIndex] = word;

            _currentWordIndex++;

            if (_currentWordIndex >= Ump.NumberOfMessageWordsFromFirstWord(_words[0]))
            {
                // construct a UMP and add it to the list
                var ump = new Ump();
                ump.Alloc(Ump.NumberOfMessageWordsFromFirstWord(_words[0]));

                for (int i = 0; i < ump.Words.Length; i++)
                {
                    ump.Words[i] = _words[i];
                }

                Umps.Enqueue(ump);

                // prepare for more words
                _currentWordIndex = 0;
            }
        }
    }
}
