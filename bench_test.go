package cgotraceback

import (
	"testing"

	"github.com/nsrip-dd/cgotraceback/internal"
)

func BenchmarkContext(b *testing.B) {
	b.Run("baseline", func(b *testing.B) {
		setEnabled(false)
		defer setEnabled(true)
		for i := 0; i < b.N; i++ {
			internal.DoCallback(func() {})
		}
	})

	b.Run("enabled", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			internal.DoCallback(func() {})
		}
	})
}
